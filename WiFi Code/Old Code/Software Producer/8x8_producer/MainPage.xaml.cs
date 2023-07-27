using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage.Streams;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;


namespace _8x8_producer
{
    public sealed partial class MainPage : Page
    {
        private readonly string _deviceAddress = "d6:c0:de:cc:8b:7a";
        private readonly string _deviceAddress1 = "fa:92:b7:a0:ae:9b";
        private BluetoothLEDevice _bleDevice;
        private GattDeviceService _service;
        private GattCharacteristic _characteristic;
        private bool _isConnected = false;

        public MainPage()
        {
            InitializeComponent();
            ConnectToDevice();
        }


        private async void ConnectToDevice()
        {
            try
            {
                _bleDevice = await BluetoothLEDevice.FromBluetoothAddressAsync(ulong.Parse(_deviceAddress1.Replace(":", ""), System.Globalization.NumberStyles.HexNumber));
                if (_bleDevice == null)
                {
                    SelectedDeviceText.Text = "Device not found";
                    return;
                }

                var accessStatus = await _bleDevice.RequestAccessAsync();
                if (accessStatus != DeviceAccessStatus.Allowed)
                {
                    SelectedDeviceText.Text = "Access denied";
                    return;
                }

                var servicesResult = await _bleDevice.GetGattServicesAsync();
                if (servicesResult.Status != GattCommunicationStatus.Success)
                {
                    SelectedDeviceText.Text = "Failed to get services";
                    return;
                }

                var services = servicesResult.Services;
                _service = services.FirstOrDefault(s => s.Uuid == new Guid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"));
                if (_service == null)
                {
                    SelectedDeviceText.Text = "Service not found";
                    return;
                }

                var characteristicsResult = await _service.GetCharacteristicsAsync();
                if (characteristicsResult.Status != GattCommunicationStatus.Success)
                {
                    SelectedDeviceText.Text = "Failed to get characteristics";
                    return;
                }

                _characteristic = characteristicsResult.Characteristics.FirstOrDefault(c => c.Uuid == new Guid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"));
                if (_characteristic == null)
                {
                    SelectedDeviceText.Text = "Characteristic not found";
                    return;
                }

                _isConnected = true;
                SelectedDeviceText.Text = _bleDevice.Name;
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex.Message);
            }
        }

        private async void sendButton_Click(object sender, RoutedEventArgs e)
        {
            if (!_isConnected)
            {
                SelectedDeviceText.Text = "Not connected to device";
                return;
            }

            try
            {
                string data = sendText.Text + Environment.NewLine; // add new line character
                var writer = new DataWriter();
                writer.WriteString(data);
                var result = await _characteristic.WriteValueAsync(writer.DetachBuffer(), GattWriteOption.WriteWithoutResponse);
                if (result != GattCommunicationStatus.Success)
                {
                    SelectedDeviceText.Text = "Failed to write value";
                    return;
                }
                SelectedDeviceText.Text = "Value written successfully";
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex.Message);
            }
        }
    }
}