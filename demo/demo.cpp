#include <iostream>
#include "wedopp.hpp"

namespace
{
    auto typeToString(wedopp::Device::Type type) noexcept
    {
        switch (type)
        {
            case wedopp::Device::Type::none: return "none";
            case wedopp::Device::Type::motor: return "motor";
            case wedopp::Device::Type::servoMotor: return "servo motor";
            case wedopp::Device::Type::light: return "light";
            case wedopp::Device::Type::distanceSensor: return "distance sensor";
            case wedopp::Device::Type::tiltSensor: return "tilt sensor";
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

                for (const auto& device : hub.getDevices())
                    std::cout << "  Device " << typeToString(device.getType()) << '\n';
            }

            for (;;)
            {
                bool enable = false;

                for (const auto& hub : hubs)
                    for (const auto& device : hub.getDevices())
                        if ((device.getType() == wedopp::Device::Type::distanceSensor || device.getType() == wedopp::Device::Type::tiltSensor) &&
                            device.getValue() <= 80U)
                            enable = true;

                for (const auto& hub : hubs)
                    for (const auto& device : hub.getDevices())
                        if (device.getType() == wedopp::Device::Type::motor ||
                            device.getType() == wedopp::Device::Type::servoMotor ||
                            device.getType() == wedopp::Device::Type::light)
                            device.setValue(enable ? 127 : 0);

#ifdef _WIN32
                Sleep(10);
#else
                sleep(10);
#endif
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
