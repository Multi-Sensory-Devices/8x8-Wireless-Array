using System;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth.Rfcomm;
using Windows.Devices.Enumeration;
using Windows.Networking.Sockets;
using Windows.Storage.Streams;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.UI;
using System.Text;
using System.Diagnostics;
using Windows.UI.Xaml.Input;

namespace ServerApp
{
    public sealed partial class MainPage : Page
    {
        private StreamSocket _socket;
        private DataReader _dataReader;
        private DataWriter _dataWriter;
        private bool _isConnected;

        public MainPage()
        {
            this.InitializeComponent();
            _isConnected = false;
            DiscoverAsync();
            UpdateConnectionStatusIndicator();
            SendTextBox.KeyUp += SendTextBox_KeyUp;

        }

        private void SendTextBox_KeyUp(object sender, KeyRoutedEventArgs e)
        {
            if (e.Key == Windows.System.VirtualKey.Enter)
            {
                SendButton_Click(sender, e);
            }
        }

        private async void ConnectButton_Click(object sender, RoutedEventArgs e)
        {
            if (!_isConnected)
            {
                await DiscoverAndConnectToDeviceAsync();
            }
            else
            {
                Disconnect();
            }
        }

        private async void SendButton_Click(object sender, RoutedEventArgs e)
        {
            if (_isConnected)
            {
                await SendMessageAsync(SendTextBox.Text);
            }
            else
            {
                await ShowMessageAsync("Not connected to any device.");
            }
        }

        private async Task DiscoverAsync()
        {
            var selector = RfcommDeviceService.GetDeviceSelector(RfcommServiceId.SerialPort);
            var devices = await DeviceInformation.FindAllAsync(selector);
            if (devices.Count > 0)
            {
                var deviceInfo = devices[0];
                btComboBox.ItemsSource = devices;
                if (btComboBox.Items.Count > 0)
                {
                    btComboBox.SelectedIndex = 0;
                }
            }
        }



        private async Task DiscoverAndConnectToDeviceAsync()
        {
            try
            {
                var selector = RfcommDeviceService.GetDeviceSelector(RfcommServiceId.SerialPort);
                var devices = await DeviceInformation.FindAllAsync(selector);

                if (devices.Count > 0)
                {

                    var deviceInfo = devices[0];
                    var service = await RfcommDeviceService.FromIdAsync(deviceInfo.Id);
                    btComboBox.ItemsSource = devices;
                    if (btComboBox.Items.Count > 0)
                    {
                        btComboBox.SelectedIndex = 0;
                    }
                    if (service != null)
                    {
                        _socket = new StreamSocket();
                        await _socket.ConnectAsync(service.ConnectionHostName, service.ConnectionServiceName);

                        _dataReader = new DataReader(_socket.InputStream);
                        _dataWriter = new DataWriter(_socket.OutputStream);

                        _isConnected = true;
                        UpdateConnectionStatusIndicator();
                        ConnectButton.Content = "Disconnect";
                       // await ShowMessageAsync("Connected to the device.");

                        _ = Task.Run(async () => { await ReceiveMessagesAsync(); });
                    }
                    else
                    {
                        await ShowMessageAsync("The service is not available on the selected device.");
                    }
                }
                else
                {
                    await ShowMessageAsync("No devices found.");
                }
            }
            catch (Exception ex)
            {
                await ShowMessageAsync("Failed to connect to the device: " + ex.Message);
            }
        }

        private async Task SendMessageAsync(string message)
        {
            if (!string.IsNullOrEmpty(message))
            {
                try
                {
                    // Convert the message to a byte array
                    byte[] messageBytes = Encoding.ASCII.GetBytes(message);

                    // Send the message over the Bluetooth connection
                    await _dataWriter.StoreAsync();
                    await _dataWriter.FlushAsync();
                    _dataWriter.WriteBytes(messageBytes);
                    await _dataWriter.StoreAsync();
                    await _dataWriter.FlushAsync();
                }
                catch (Exception ex)
                {
                    await ShowMessageAsync("Failed to send the message: " + ex.Message);
                }
            }
        }

        private async Task ReceiveMessagesAsync()
        {
            try
            {
                while (_isConnected)
                {
                    uint size = await _dataReader.LoadAsync(1); // set as packet size
                    string receivedMessage = _dataReader.ReadString(size);
                    await UpdateReceivedTextBlockAsync(receivedMessage);
                }
            }
            catch (Exception ex)
            {
                if (_isConnected)
                {
                    await ShowMessageAsync("An error occurred while receiving data: " + ex.Message);
                }
            }
        }

        private async Task UpdateReceivedTextBlockAsync(string message)
        {
            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                ReceivedTextBlock.Text += message;
            });
        }

        private async Task ShowMessageAsync(string message)
        {
            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
            {
                var dialog = new ContentDialog
                {
                    Title = "Message",
                    Content = message,
                    CloseButtonText = "OK"
                };
                await dialog.ShowAsync();
            });
        }

        private void Disconnect()
        {
            _isConnected = false;
            UpdateConnectionStatusIndicator();
            ConnectButton.Content = "Connect";

            _dataReader?.Dispose();
            _dataReader = null;

            _dataWriter?.Dispose();
            _dataWriter = null;

            _socket?.Dispose();
            _socket = null;
        }

        private void UpdateConnectionStatusIndicator()
        {
            ConnectionStatusIndicator.Fill = _isConnected ? new SolidColorBrush(Colors.Green) : new SolidColorBrush(Colors.Red);
        }

        private void ClearButton_Click(object sender, RoutedEventArgs e)
        {
            ReceivedTextBlock.Text = string.Empty;
        }

        private async void RefreshButton_Click(object sender, RoutedEventArgs e)
        {
            await DiscoverAsync();
        }
    }
}
