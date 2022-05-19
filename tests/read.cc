#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

#include "../timing.h"

double findMedian(long double a[], int n) {
    std::sort(a, a + n);

    if (n % 2 != 0)
        return (double)a[n / 2];

    return (double)(a[(n - 1) / 2] + a[n / 2]) / 2.0;
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("Usage : ./read <file_path>\n");
        return 0;
    }
    int size_contents = std::stoi(argv[2]);
    char *contents = new char[size_contents];
    int n = 20;
    char* filepath_array = argv[1];
    long double start, end, totalStart, totalEnd;
    long double open_time[n], read_time[n], close_time[n], total[n];
    int fd;
    for (int i = 0; i < n; i++) {
        totalStart = time_monotonic();
        start = totalStart;
        fd = open(filepath_array, O_RDWR);
        end = time_monotonic();
        open_time[i] = end - start;

        if (fd != -1) {
            start = time_monotonic();
            read(fd, contents, size_contents);
            end = time_monotonic();
            read_time[i] = end - start;
        }

        start = time_monotonic();
        close(fd);
        end = time_monotonic();
        close_time[i] = end - start;

        totalEnd = end;
        total[i] = totalEnd - totalStart;
    }

    std::cout << std::fixed << "open,read,close,total" << "\n";
    for (int i = 0; i < n; i++) {
        std::cout << std::fixed << open_time[i] << "," << read_time[i] << "," << close_time[i] << "," << total[i] << "\n";
    }
    std::cout << std::fixed << "First," << open_time[0] << "," << read_time[0] << "," << close_time[0] << "," << total[0] << "\n";
    std::cout << std::fixed << "Median," << findMedian(open_time, n) << "," << findMedian(read_time, n) << "," << findMedian(close_time, n) << "," << findMedian(total, n) << "\n";

}