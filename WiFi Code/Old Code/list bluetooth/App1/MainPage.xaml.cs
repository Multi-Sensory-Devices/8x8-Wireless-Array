using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using System.Diagnostics;
using Windows.Devices.Bluetooth;
using Windows.Devices.Enumeration;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace App1
{
    public sealed partial class MainPage : Page, INotifyPropertyChanged
    {
        private string _selectedDeviceId;
        public string SelectedDeviceId
        {
            get { return _selectedDeviceId; }
            set
            {
                _selectedDeviceId = value;
                OnPropertyChanged();
            }
        }

        public MainPage()
        {
            InitializeComponent();

            EnumerateBluetoothDevices();
        }
        private void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            // Get the device ID from the text box
            SelectedDeviceId = DeviceIdTextBox.Text;

            // TODO: Connect to the device with the specified ID
        }

        private async void EnumerateBluetoothDevices()
        {
            var devices = await DeviceInformation.FindAllAsync(BluetoothLEDevice.GetDeviceSelector());

            foreach (var device in devices)
            {
                string deviceInfo = $"Device ID: {device.Id}, Name: {device.Name}\n";
                DeviceList.Text += deviceInfo;
            }
        }


        private void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        public event PropertyChangedEventHandler PropertyChanged;
    }
}