#include "Util.hpp"
#include <iostream>

void printBoundary(){
    std::cerr << "--------------------------------------------" << std::endl;
}

CerrRedirect::CerrRedirect(std::ostream& newStream)
    : oldBuffer(std::cerr.rdbuf(newStream.rdbuf())) {}

CerrRedirect::~CerrRedirect() {
    std::cerr.rdbuf(oldBuffer);
}