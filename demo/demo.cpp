#include <iostream>
#include "wedopp.hpp"

int main()
{
    try
    {
        const auto hubs = wedopp::findHubs();

        if (!hubs.empty())
        {
            std::cout << "LEGO WeDo hubs:\n";

            for (const auto& hub : hubs)
                std::cout << "Hub at " << hub.getPath() << '\n';
        }
        else
            std::cout << "No WeDo hubs found\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
