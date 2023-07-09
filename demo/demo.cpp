#include <iostream>
#include "wedopp.hpp"

namespace
{
    auto typeToString(wedopp::Device::Type type) noexcept
    {
        switch (type)
        {
            case wedopp::Device::none: return "none";
            case wedopp::Device::motor: return "motor";
            case wedopp::Device::servoMotor: return "servo motor";
            case wedopp::Device::light: return "light";
            case wedopp::Device::distanceSensor: return "distance sensor";
            case wedopp::Device::tiltSensor: return "tilt sensor";
            default: return "unknown";
        }
    }
}

int main()
{
    try
    {
        const auto hubs = wedopp::findHubs();

        if (!hubs.empty())
        {
            std::cout << "LEGO WeDo hubs:\n";

            for (const auto& hub : hubs)
            {
                std::cout << "Hub at " << hub.getPath() << '\n';

                const auto devices = hub.getDevices();

                for (const auto& device: devices)
                    std::cout << "  Device " << typeToString(device.getType()) << '\n';
            }
        }
        else
            std::cout << "No WeDo hubs found\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
