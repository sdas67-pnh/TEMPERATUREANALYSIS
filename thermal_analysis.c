#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <cairo.h>

#define MAX_DATA_POINTS 500
#define MAX_LINE_LENGTH 100
#define CSV_FILE "temperature_data.csv"
#define CIRCUIT_IMAGE_FILE "circuit_diagram.jpg"

typedef struct {
    double time; // Time in hours
    double temperature;
} DataPoint;

typedef struct {
    double *x_values;
    double *y_values;
    double *euler_values;
    double *errors;
    int num_points;
    double x_min, x_max, y_min, y_max;
    double total_error;
} GraphData;

DataPoint data[MAX_DATA_POINTS];
GraphData graph_data;
GtkListStore *list_store = NULL;
GtkTreeView *tree_view = NULL;

double time_string_to_hours(const char *time_str) {
    int hours, minutes;
    double seconds;
    if (sscanf(time_str, "%d:%d:%lf", &hours, &minutes, &seconds) == 3) {
        return hours + (minutes / 60.0) + (seconds / 3600.0);
    } else {
        fprintf(stderr, "Error parsing time string: %s\n", time_str);
        return 0.0;
    }
}

int read_csv_data() {
    FILE *file = fopen(CSV_FILE, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    if (fgets(line, MAX_LINE_LENGTH, file) == NULL) {
        fclose(file);
        return -1;
    }

    int count = 0;
    while (fgets(line, MAX_LINE_LENGTH, file) && count < MAX_DATA_POINTS) {
        char *time_str = strtok(line, ",");
        char *temperature_str = strtok(NULL, "\n");
        if (time_str && temperature_str) {
            data[count].time = time_string_to_hours(time_str);
            data[count].temperature = atof(temperature_str);
            count++;
        } else {
            fprintf(stderr, "Error parsing line: %s", line);
        }
    }
    fclose(file);
    return count;
}

double cooling_model(double t, double T) {
    const double k = 0.1;
    const double T_ambient = 25.0;
    return -k * (T - T_ambient);
}

void calculate_euler_predictions() {
    if (graph_data.num_points <= 1) return;
    const double h = (data[graph_data.num_points-1].time - data[0].time) / (graph_data.num_points - 1);
    graph_data.euler_values[0] = data[0].temperature;
    
    for (int i = 1; i < graph_data.num_points; i++) {
        graph_data.euler_values[i] = graph_data.euler_values[i-1] +
                                   h * cooling_model(data[i-1].time, graph_data.euler_values[i-1]);
    }
}

void calculate_errors() {
    graph_data.total_error = 0.0;
    for (int i = 0; i < graph_data.num_points; i++) {
        double absolute_error = fabs(graph_data.y_values[i] - graph_data.euler_values[i]);
        graph_data.errors[i] = (absolute_error / fabs(graph_data.y_values[i])) * 100.0;
        graph_data.total_error += graph_data.errors[i];
    }
}

void calculate_data_ranges() {
    if (graph_data.num_points == 0) return;
    graph_data.x_min = graph_data.x_values[0];
    graph_data.x_max = graph_data.x_values[0];
    graph_data.y_min = fmin(graph_data.y_values[0], graph_data.euler_values[0]);
    graph_data.y_max = fmax(graph_data.y_values[0], graph_data.euler_values[0]);
    
    for (int i = 1; i < graph_data.num_points; i++) {
        graph_data.x_min = fmin(graph_data.x_min, graph_data.x_values[i]);
        graph_data.x_max = fmax(graph_data.x_max, graph_data.x_values[i]);
        graph_data.y_min = fmin(graph_data.y_min, fmin(graph_data.y_values[i], graph_data.euler_values[i]));
        graph_data.y_max = fmax(graph_data.y_max, fmax(graph_data.y_values[i], graph_data.euler_values[i]));
    }
    
    const double padding = 0.05;
    graph_data.x_min -= (graph_data.x_max - graph_data.x_min) * padding;
    graph_data.x_max += (graph_data.x_max - graph_data.x_min) * padding;
    graph_data.y_min -= (graph_data.y_max - graph_data.y_min) * padding;
    graph_data.y_max += (graph_data.y_max - graph_data.y_min) * padding;
}

static gboolean draw_graph(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    const int width = gtk_widget_get_allocated_width(widget);
    const int height = gtk_widget_get_allocated_height(widget);
    
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 50, height-50);
    cairo_line_to(cr, width-50, height-50);
    cairo_move_to(cr, 50, 50);
    cairo_line_to(cr, 50, height-50);
    cairo_stroke(cr);
    
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, width/2, height-20);
    cairo_show_text(cr, "Time (hours)");
    cairo_save(cr);
    cairo_rotate(cr, -M_PI/2);
    cairo_move_to(cr, -height/2, 20);
    cairo_show_text(cr, "Temperature (°C)");
    cairo_restore(cr);
    
    const double x_scale = (width-100)/(graph_data.x_max - graph_data.x_min);
    const double y_scale = (height-100)/(graph_data.y_max - graph_data.y_min);
    
    cairo_set_source_rgb(cr, 0, 0.7, 0);
    for (int i = 0; i < graph_data.num_points; i++) {
        const int x = 50 + (graph_data.x_values[i] - graph_data.x_min) * x_scale;
        const int y_actual = height - 50 - (graph_data.y_values[i] - graph_data.y_min) * y_scale;
        const int y_euler = height - 50 - (graph_data.euler_values[i] - graph_data.y_min) * y_scale;
        const double error_height = fabs(y_actual - y_euler);
        cairo_rectangle(cr, x-2, fmin(y_actual, y_euler), 4, error_height);
        cairo_fill(cr);
    }
    
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_set_line_width(cr, 2);
    for (int i = 0; i < graph_data.num_points; i++) {
        const int x = 50 + (graph_data.x_values[i] - graph_data.x_min) * x_scale;
        const int y = height - 50 - (graph_data.y_values[i] - graph_data.y_min) * y_scale;
        i == 0 ? cairo_move_to(cr, x, y) : cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
    cairo_set_source_rgb(cr, 0, 0, 1);
    cairo_set_line_width(cr, 2);
    for (int i = 0; i < graph_data.num_points; i++) {
        const int x = 50 + (graph_data.x_values[i] - graph_data.x_min) * x_scale;
        const int y = height - 50 - (graph_data.euler_values[i] - graph_data.y_min) * y_scale;
        i == 0 ? cairo_move_to(cr, x, y) : cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
    cairo_set_font_size(cr, 12);
    const char* legends[] = {"Actual Data", "Euler Prediction", "Error Magnitude"};
    const double colors[][3] = {{1,0,0}, {0,0,1}, {0,0.7,0}};
    for (int i = 0; i < 3; i++) {
        cairo_set_source_rgb(cr, colors[i][0], colors[i][1], colors[i][2]);
        cairo_rectangle(cr, width-150, 60 + i*30, 15, 15);
        cairo_fill(cr);
        cairo_move_to(cr, width-130, 73 + i*30);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_show_text(cr, legends[i]);
    }
    
    cairo_set_font_size(cr, 10);
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_move_to(cr, width - 300, height - 20);
    cairo_show_text(cr, "T(t) = T_ambient + (T_initial - T_ambient) * e^(-k * t)");
    
    return FALSE;
}

void update_data_table() {
    gtk_list_store_clear(list_store);
    for (int i = 0; i < graph_data.num_points; i++) {
        int rounded_minutes = (int)round(data[i].time * 60);
        if (rounded_minutes % 10 == 0) {
            GtkTreeIter iter;
            gtk_list_store_append(list_store, &iter);
            gtk_list_store_set(list_store, &iter,
                              0, i + 1,
                              1, graph_data.x_values[i],
                              2, graph_data.y_values[i],
                              3, graph_data.euler_values[i],
                              4, graph_data.errors[i],
                              -1);
        }
    }
}

void create_data_table(GtkWidget *parent_window) {
    GtkWidget *data_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(data_window), "Data Table");
    gtk_window_set_default_size(GTK_WINDOW(data_window), 600, 400);
    
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(data_window), scrolled_window);
    
    list_store = gtk_list_store_new(5, G_TYPE_INT, G_TYPE_DOUBLE,
                                   G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
    tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store)));
    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(tree_view));
    
    const char* headers[] = {"#", "Time (hours)", "Actual (°C)", "Euler (°C)", "Error (%)"};
    for (int i = 0; i < 5; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(headers[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(tree_view, col);
        gtk_tree_view_column_set_sort_column_id(col, i);
    }
    
    GtkWidget *error_label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(data_window), error_label);
    gtk_label_set_markup(GTK_LABEL(error_label),
                        g_strdup_printf("<b>Total Absolute Error:</b> %.4f %%", graph_data.total_error));
    
    gtk_widget_show_all(data_window);
    update_data_table();
}

void show_circuit_image(GtkWidget *widget, gpointer data) {
    GtkWidget *image_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(image_window), "Circuit Diagram");
    gtk_window_set_default_size(GTK_WINDOW(image_window), 600, 400);
    
    GtkWidget *image = gtk_image_new_from_file(CIRCUIT_IMAGE_FILE);
    if (image == NULL) {
        fprintf(stderr, "Failed to load circuit image: %s\n", CIRCUIT_IMAGE_FILE);
        gtk_widget_destroy(image_window);
        return;
    }
    
    gtk_container_add(GTK_CONTAINER(image_window), image);
    gtk_widget_show_all(image_window);
}

void initialize_application() {
    int num_points = read_csv_data();
    if (num_points <= 0) {
        fprintf(stderr, "Failed to load data. Check CSV file.\n");
        exit(EXIT_FAILURE);
    }
    
    graph_data.num_points = num_points;
    graph_data.x_values = malloc(num_points * sizeof(double));
    graph_data.y_values = malloc(num_points * sizeof(double));
    graph_data.euler_values = malloc(num_points * sizeof(double));
    graph_data.errors = malloc(num_points * sizeof(double));

    for (int i = 0; i < num_points; i++) {
        graph_data.x_values[i] = data[i].time;
        graph_data.y_values[i] = data[i].temperature;
    }

    calculate_euler_predictions();
    calculate_errors();
    calculate_data_ranges();
}

void cleanup_application() {
    free(graph_data.x_values);
    free(graph_data.y_values);
    free(graph_data.euler_values);
    free(graph_data.errors);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Thermal Analysis System");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 800);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_box);
    
    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(main_box), notebook, TRUE, TRUE, 0);
    
    GtkWidget *graph_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), graph_tab, gtk_label_new("Graph"));
    
    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, -1, 500);
    gtk_box_pack_start(GTK_BOX(graph_tab), drawing_area, TRUE, TRUE, 0);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(draw_graph), NULL);
    
    GtkWidget *data_table_button = gtk_button_new_with_label("Show Data Table");
    g_signal_connect(data_table_button, "clicked", G_CALLBACK(create_data_table), window);
    gtk_box_pack_start(GTK_BOX(main_box), data_table_button, FALSE, FALSE, 5);
    
    GtkWidget *circuit_button = gtk_button_new_with_label("View Circuit Diagram");
    g_signal_connect(circuit_button, "clicked", G_CALLBACK(show_circuit_image), NULL);
    gtk_box_pack_start(GTK_BOX(main_box), circuit_button, FALSE, FALSE, 5);
    
    initialize_application();
    gtk_widget_show_all(window);
    gtk_main();
    cleanup_application();
    
    return 0;
}
