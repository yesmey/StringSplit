#include <iostream>
#include "string_splitter.h"

int main()
{
    std::string ptr = "this,is,a,string,separated,,,,,,by*stars*and,commas";
    const std::vector<meyer::StringPiece> splitted = meyer::split<',', '*'>(ptr);
    for (const auto& itr : splitted) {
        if (!itr.empty()) {
            std::cout.write(itr, itr.size());
            std::cout << '\n';
        }
    }

    return 0;
}
