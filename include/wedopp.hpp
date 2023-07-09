#ifndef WEDOPP_HPP
#define WEDOPP_HPP

#include <array>
#include <memory>
#include <system_error>
#include <vector>
#ifdef _WIN32
#  pragma push_macro("WIN32_LEAN_AND_MEAN")
#  pragma push_macro("NOMINMAX")
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif // WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif // NOMINMAX
#  include <Windows.h>
#  include <hidsdi.h>
#  include <SetupAPI.h>
#  pragma pop_macro("WIN32_LEAN_AND_MEAN")
#  pragma pop_macro("NOMINMAX")
#endif

namespace wedopp
{
    namespace detail
    {
#ifdef _WIN32
        class InterfaceDetailData final
        {
        public:
            InterfaceDetailData(const std::size_t size):
                data{static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(std::malloc(size))}
            {
                if (!data)
                    throw std::bad_alloc{};

                data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
            }

            ~InterfaceDetailData()
            {
                std::free(data);
            }

            InterfaceDetailData(InterfaceDetailData&& other) noexcept: data{other.data}
            {
                other.data = nullptr;
            }

            InterfaceDetailData& operator=(InterfaceDetailData&& other) noexcept
            {
                if (this != &other)
                {
                    data = other.data;
                    other.data = nullptr;
                }
                return *this;
            }

            [[nodiscard]] auto get() const noexcept { return data; }

            const auto operator->() const noexcept { return data; }
            auto operator->() noexcept { return data; }

        private:
            SP_DEVICE_INTERFACE_DETAIL_DATA_A* data = nullptr;
        };

        class File final
        {
        public:
            File(LPSTR filename, const DWORD desiredAccess, const DWORD shareMode, const DWORD creationDisposition, const DWORD flagsAndAttributes):
                handle{CreateFileA(filename, desiredAccess, shareMode, nullptr, creationDisposition, flagsAndAttributes, nullptr)}
            {
                if (handle == INVALID_HANDLE_VALUE)
                    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), "Failed to open file"};
            }

            ~File()
            {
                if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
            }

            File(File&& other) noexcept: handle{other.handle}
            {
                other.handle = INVALID_HANDLE_VALUE;
            }

            File& operator=(File&& other) noexcept
            {
                if (this != &other)
                {
                    handle = other.handle;
                    other.handle = INVALID_HANDLE_VALUE;
                }
                return *this;
            }

            [[nodiscard]] auto get() const noexcept { return handle; }

            template <std::size_t n>
            void write(const std::array<std::uint8_t, n>& data) const
            {
                if (!WriteFile(handle, data.data(), static_cast<DWORD>(data.size()), nullptr, nullptr))
                    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), "Failed to write to file"};
            }

            template <std::size_t n>
            void read(std::array<std::uint8_t, n>& data) const
            {
                if (!ReadFile(handle, data.data(), static_cast<DWORD>(data.size()), nullptr, nullptr))
                    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), "Failed to read from file"};
            }

        private:
            HANDLE handle = INVALID_HANDLE_VALUE;
        };

        class DevInfo final
        {
        public:
            DevInfo(const GUID* guid, const DWORD flags):
                classGuid{guid},
                handle{SetupDiGetClassDevs(classGuid, nullptr, nullptr, flags)}
            {
                if (handle == INVALID_HANDLE_VALUE)
                    throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), "Failed to get devices"};
            }

            ~DevInfo()
            {
                if (handle != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(handle);
            }

            DevInfo(DevInfo&& other) noexcept: handle{other.handle}
            {
                other.handle = INVALID_HANDLE_VALUE;
            }

            DevInfo& operator=(DevInfo&& other) noexcept
            {
                if (this != &other)
                {
                    handle = other.handle;
                    other.handle = INVALID_HANDLE_VALUE;
                }
                return *this;
            }

            std::vector<InterfaceDetailData> enumInterfaces()
            {
                std::vector<InterfaceDetailData> interfaces;

                for (DWORD index = 0;; ++index)
                {
                    SP_DEVICE_INTERFACE_DATA interfaceData{};
                    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

                    if (!SetupDiEnumDeviceInterfaces(handle, nullptr, classGuid, index, &interfaceData))
                    {
                        if (const auto error = GetLastError(); error != ERROR_NO_MORE_ITEMS)
                            throw std::system_error{static_cast<int>(error), std::system_category(), "Failed to enumerate device interfaces"};
                        else
                            break;
                    }

                    DWORD requiredLength = 0;
                    if (!SetupDiGetDeviceInterfaceDetailA(handle, &interfaceData, nullptr, 0, &requiredLength, nullptr))
                        if (const auto error = GetLastError(); error != ERROR_INSUFFICIENT_BUFFER)
                            throw std::system_error{static_cast<int>(error), std::system_category(), "Failed to get interface detail"};

                    InterfaceDetailData interfaceDetailData{requiredLength};

                    if (!SetupDiGetDeviceInterfaceDetailA(handle, &interfaceData, interfaceDetailData.get(), requiredLength, &requiredLength, nullptr))
                        throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), "Failed to get interface detail"};

                    interfaces.push_back(std::move(interfaceDetailData));
                }

                return interfaces;
            }

        private:
            const GUID* classGuid = nullptr;
            HDEVINFO handle = INVALID_HANDLE_VALUE;
        };
#endif

        class Processor final
        {
        public:
            Processor(File f) noexcept: file{std::move(f)}{}

            std::pair<std::uint8_t, std::uint8_t> readDeviceTypes()
            {
                file.write(writeBuffer);
                file.read(readBuffer);

                return {readBuffer[4], readBuffer[6]};
            }

            void writeValue(std::uint8_t slot, std::uint8_t value)
            {
                writeBuffer[1U] = 64U;
                writeBuffer[2U + slot] = value;
                file.write(writeBuffer);
            }

            std::uint8_t readValue(std::uint8_t slot)
            {
                file.read(readBuffer);
                return readBuffer[3U + slot * 2U];
            }

        private:
            detail::File file;
            std::array<std::uint8_t, 9> readBuffer{};
            std::array<std::uint8_t, 9> writeBuffer{};
        };
    }

    class Device final
    {
    public:
        enum class Type: std::uint8_t
        {
            none,
            motor,
            servoMotor,
            light,
            distanceSensor,
            tiltSensor
        };

        Device(const Type t, const std::uint8_t s, detail::Processor* p) noexcept:
            type{t}, slot{s}, processor{p}
        {}

        [[nodiscard]] auto getType() const noexcept { return type; }
        [[nodiscard]] auto getSlot() const noexcept { return slot; }

        void setValue(std::uint8_t value) const
        {
            processor->writeValue(slot, value);
        }

        std::uint8_t getValue() const
        {
            return processor->readValue(slot);
        }
        
    private:
        Type type = Type::none;
        std::uint8_t slot = 0;
        detail::Processor* processor = nullptr;
    };

    class Hub final
    {
    public:
        Hub(std::string p, detail::File f): path{std::move(p)}, processor{new detail::Processor(std::move(f))}
        {
            const auto deviceTypes = processor->readDeviceTypes();

            devices.push_back(Device{getDeviceType(deviceTypes.first), 0U, processor.get()});
            devices.push_back(Device{getDeviceType(deviceTypes.second), 1U, processor.get()});
        }

        [[nodiscard]] const auto& getPath() const noexcept { return path; }

        [[nodiscard]] const auto& getDevices() const noexcept { return devices; }

    private:
        static Device::Type getDeviceType(std::uint8_t b)
        {
            std::cout << static_cast<uint32_t>(b) << '\n';

            switch (b)
            {
                case 38U:
                case 39U:
                    return Device::Type::tiltSensor;

                case 102U:
                case 103U:
                    return Device::Type::servoMotor;

                case 177U:
                case 178U:
                case 179U:
                case 180U:
                    return Device::Type::distanceSensor;
                    
                case 202U:
                case 203U:
                case 204U:
                case 205U:
                    return Device::Type::light;

                case 231U:
                    return Device::Type::none;

                case 239U:
                case 240U:
                case 241U:
                    return Device::Type::motor;

                default: return Device::Type::none;
            }
        }

        std::string path;
        std::vector<Device> devices;
        std::unique_ptr<detail::Processor> processor;
    };

    [[nodiscard]] std::vector<Hub> findHubs()
    {
        std::vector<Hub> hubs;

        GUID hidGuid;
        HidD_GetHidGuid(&hidGuid);
        detail::DevInfo devInfo{&hidGuid, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE | DIGCF_ALLCLASSES};

#ifdef _WIN32
        const auto interfaces = devInfo.enumInterfaces();
        for (const auto& interface : interfaces)
        {
            try
            {
                detail::File file{interface->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH};

                constexpr std::int16_t productId = 0x0003;
                constexpr std::int16_t vendorId = 0x0694;

                HIDD_ATTRIBUTES attributes{};
                attributes.Size = sizeof(attributes);
                if (HidD_GetAttributes(file.get(), &attributes) &&
                    attributes.VendorID == vendorId && attributes.ProductID == productId)
                    hubs.push_back(Hub{interface->DevicePath, std::move(file)});
            }
            catch (const std::system_error&)
            {
            }
        }
#else
#endif

        return hubs;
    }
}

#endif
