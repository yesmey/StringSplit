#include "string_splitter.h"
#include <iostream>


int main(int args, char** argsv)
{
    std::string ptr = "this,is,a,string,separated,,,,,,by*stars*and,commas" + args;
    std::vector<msu::StringPiece> vec;

    msu::split<',', '*'>(vec, ptr);
    for (const auto& itr : vec) {
        if (!itr.empty()) {
            std::cout.write(itr, itr.size());
            std::cout << '\n';
        }
    }

    return 0;
}
