#include <iostream>
#include <boost/filesystem.hpp>
int main()
{
	boost::filesystem::path path = "./abc/de.txt";
	std::cout << path.extension() << std::endl;
}