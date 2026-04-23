#include <iostream>

int main(int argc, char *argv[]){
    if (argc <= 1) {
        std::cout << "No arguments provided." << std::endl;
        return 1;
    }

    std::cout << argv[1] << std::endl;
}