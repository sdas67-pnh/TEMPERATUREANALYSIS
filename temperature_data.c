#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROWS 300  // Adjust this based on the maximum number of rows in your CSV
#define FILENAME "temperature_data.csv" // Define the filename

int main() {
    FILE *fp;
    char line[100]; // Assuming each line is no more than 100 characters
    double time[MAX_ROWS];
    double temperature[MAX_ROWS];
    int row = 0;

    fp = fopen(FILENAME, "r");
    if (fp == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Skip the header line
    fgets(line, sizeof(line), fp);

    // Read data line by line
    while (fgets(line, sizeof(line), fp) != NULL && row < MAX_ROWS) {
        double t, temp;
        if (sscanf(line, "%lf,%lf", &t, &temp) == 2) {
            time[row] = t;
            temperature[row] = temp;
            row++;
        } else {
            fprintf(stderr, "Warning: Could not parse line: %s", line);
        }
    }

    fclose(fp);

    // Print the data (for verification)
    printf("Time (s), Temperature (°C)\n");
    for (int i = 0; i < row; i++) {
        printf("%lf, %lf\n", time[i], temperature[i]);
    }

    return 0;
}

