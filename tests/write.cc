#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
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
    if(argc < 3) {
        printf("Usage : ./write <file_path> <size_bytes>\n");
        return 0;
    }
    char* contents = new char[std::stoi(argv[2])];
    int n = 2;
    std::string filepath = std::string(argv[1]);
    char* filepath_array = argv[1];
    long double start, end, totalStart, totalEnd;
    long double createtime[n], opentime[n], writetime[n], closetime[n], totaltime[n];

    for (int i = 0; i < n; i++) {
        totalStart = time_monotonic();
        start = totalStart;
        int fd = creat(filepath_array, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        end = time_monotonic();
        createtime[i] = end - start;

        //start = time_monotonic();
        //fd = open(filepath_array, O_RDWR);
        //end = time_monotonic();
        //opentime[i] = end - start;

        start = time_monotonic();
        write(fd, contents, std::stoi(argv[2]));
        end = time_monotonic();
        writetime[i] = end - start;

        start = time_monotonic();
        close(fd);
        end = time_monotonic();
        closetime[i] = end - start;

        totalEnd = end;
        totaltime[i] = totalEnd - totalStart;

        ::remove(filepath.c_str());
    }

    std::cout << std::fixed << "creat,open,write,close,total" << "\n";
    for (int i = 0; i < n; i++) {
        std::cout << std::fixed << createtime[i] << "," << opentime[i] << "," << writetime[i] << "," << closetime[i] << "," << totaltime[i] << "\n";
    }
    std::cout << std::fixed << "First," << createtime[0] << "," << opentime[0] << "," << writetime[0] << "," << closetime[0] << "," << totaltime[0] << "\n";
    std::cout << std::fixed << "Median," << findMedian(createtime, n) << "," << findMedian(opentime, n) << ","
    << findMedian(writetime, n) << "," << findMedian(closetime, n) << "," << findMedian(totaltime, n) << "\n";

}
