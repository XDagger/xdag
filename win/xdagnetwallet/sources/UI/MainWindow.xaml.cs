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
using System.Windows.Navigation;
using System.Windows.Shapes;
using XDagNetWallet.Components;
using XDagNetWalletCLI;

namespace XDagNetWallet.UI
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private XDagWallet xDagWallet = null;
        private XDagRuntime xDagRuntime = null;

        public MainWindow()
        {
            InitializeComponent();
        }

        #region UI Component Methods

        private void btnStart_Click(object sender, RoutedEventArgs e)
        {
            if (xDagRuntime == null)
            {
                return;
            }

            xDagRuntime.Start();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            xDagWallet = new XDagWallet();
            xDagWallet.SetPromptInputPasswordFunction((prompt, passwordSize) =>
            {
                return InputPassword(prompt, passwordSize);
            });

            xDagWallet.SetUpdateStateFunction((state, balance, address) =>
            {
                return UpdateState(state, balance, address);
            });

            xDagRuntime = new XDagRuntime(xDagWallet);
        }

        #endregion

        public String InputPassword(String promptMessage, uint passwordSize)
        {
            string result = string.Empty;
            PasswordWindow passwordWindow = new PasswordWindow(promptMessage, (passwordInput) => {
                result = passwordInput;
            });
            passwordWindow.ShowDialog();

            return result;
        }

        public int UpdateState(String state, String balance, String address)
        {
            // MessageBox.Show(string.Format("Get State Update: State=[{0}], Balance=[{1}], Address=[{2}]", state, balance, address));

            tbxAccountAddress.Text = address;
            tbxBalance.Text = balance;

            lblStatus.Content = state;

            return 0;
        }

        private void btnCheckAccount_Click(object sender, RoutedEventArgs e)
        {

            if (xDagRuntime.HasExistingAccount())
            {
                MessageBox.Show("Yes");
            }
            else
            {
                MessageBox.Show("No");
            }
        }
    }
}
