using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using XDagNetWallet.Components;
using XDagNetWalletCLI;

namespace XDagNetWallet.UI.Windows
{
    /// <summary>
    /// Interaction logic for WalletWindow.xaml
    /// </summary>
    public partial class WalletWindow : Window
    {
        private XDagWallet xDagWallet = null;

        private XDagRuntime xDagRuntime = null;

        public WalletWindow(XDagWallet wallet)
        {
            if (wallet == null)
            {
                throw new ArgumentNullException("wallet is null.");
            }

            xDagWallet = wallet;

            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            xDagRuntime = new XDagRuntime(xDagWallet);
            
            xDagWallet.SetStateChangedAction((state) =>
            {
                this.Dispatcher.Invoke(() =>
                {
                    OnUpdatingState(state);
                });
            });

            xDagWallet.SetAddressChangedAction((address) =>
            {
                this.Dispatcher.Invoke(() =>
                {
                    OnUpdatingAddress(address);
                });
            });

            xDagWallet.SetBalanceChangedAction((balance) =>
            {
                this.Dispatcher.Invoke(() =>
                {
                    OnUpdatingBalance(balance);
                });
            });

            OnUpdatingState(xDagWallet.State);
            OnUpdatingAddress(xDagWallet.Address);
            OnUpdatingBalance(xDagWallet.Balance);
        }

        private void OnUpdatingState(WalletState state)
        {
            lblWalletStatus.Content = state.ToString();

            if (state == WalletState.ConnectedPool)
            {
                ellPortrait.Fill = new SolidColorBrush(Colors.Green);
            }
        }

        private void OnUpdatingAddress(string address)
        {
            this.lblWalletAddress.Content = string.IsNullOrEmpty(address) ? "[N/A]" : address;
        }

        private void OnUpdatingBalance(double balance)
        {
            this.lblWalletBalance.Content = XDagWallet.BalanceToString(balance);
        }
    }
}
