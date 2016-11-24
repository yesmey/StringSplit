#include <iostream>
#include <string>
#include <vector>
#include "string_splitter.h"

int main()
{
    std::string ptr = "this,is,a,string,separated,,,,,,by*stars*and,commas";
    std::vector<meyer::StringPiece> vec;

    meyer::split<',', '*'>(vec, ptr);
    for (const auto& itr : vec) {
        if (!itr.empty()) {
            std::cout.write(itr, itr.size());
            std::cout << '\n';
        }
    }

    return 0;
}
