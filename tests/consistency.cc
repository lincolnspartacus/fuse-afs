#include <iostream>
#include <vector>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include "../timing.h"

using namespace std;

int main(int argc, char** argv) {

    if (argc < 2) {
        printf("Usage : ./read <file_path>\n");
        return 0;
    }
    int size_contents = std::stoi(argv[2]);
    char *contents = new char[size_contents];
    int n = 10;
    char *filepath_array = argv[1];
    long double start, end, totalStart, totalEnd;
    long double open_times[n];
    int fd;

    for (int i = 0; i < n; i++) {

        start = time_monotonic();
        fd = open(filepath_array, O_RDWR);
        open_times[i] = time_monotonic() - start;
        read(fd, contents, size_contents);

        close(fd);

        cout << "next?";
        cin.get();
    }

    for (int i = 0; i < n; i++) {
        cout << open_times[i] << '\n';
    }
}