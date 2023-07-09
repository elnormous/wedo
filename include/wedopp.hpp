#ifndef WEDOPP_HPP
#define WEDOPP_HPP

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
                data{static_cast<_SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(std::malloc(size))}
            {
                if (!data)
                    throw std::bad_alloc{};

                data->cbSize = sizeof(_SP_DEVICE_INTERFACE_DETAIL_DATA_A);
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
            _SP_DEVICE_INTERFACE_DETAIL_DATA_A* data = nullptr;
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
            const GUID* classGuid;
            HDEVINFO handle = INVALID_HANDLE_VALUE;
        };
#endif
    }

    class Device final
    {
    public:
    };

    class Hub final
    {
    public:
        Hub(std::string p, detail::File f): path{std::move(p)}, file{std::move(f)} {}

        [[nodiscard]] const auto& getPath() const noexcept { return path; }

    private:
        std::string path;
        detail::File file;
    };

    std::vector<Hub> findHubs()
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

                HIDD_ATTRIBUTES attributes;
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
