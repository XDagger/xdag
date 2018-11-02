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
using XDagNetWallet.UI.Async;
using XDagNetWalletCLI;

namespace XDagNetWallet.UI.Windows
{
    /// <summary>
    /// Interaction logic for LogonWindow.xaml
    /// </summary>
    public partial class LogonWindow : Window
    {
        private enum LogonStatus
        {
            None,
            Registering,
            RegisteringConfirmPassword,
            RegisteringRandomKeys,
            RegisteringPending,
            Connecting,
            ConnectingPending,
        }

        private LogonStatus logonStatus = LogonStatus.None;

        private XDagWallet xDagWallet = null;
        private XDagRuntime runtime = null;

        private string userInputPassword = string.Empty;

        public LogonWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            xDagWallet = new XDagWallet();
            runtime = new XDagRuntime(xDagWallet);

            xDagWallet.SetPromptInputPasswordFunction((prompt, passwordSize) =>
            {
                return this.Dispatcher.Invoke(() =>
                {
                    return InputPassword(prompt, passwordSize);
                });
            });

            xDagWallet.SetUpdateStateFunction((state, balance, address) =>
            {
                return this.Dispatcher.Invoke(() =>
                {
                    return UpdateState(state, balance, address);
                });
            });

            xDagWallet.SetStateChangedAction((state) =>
            {
                this.Dispatcher.Invoke(() =>
                {
                    UpdateState(state);
                });
            });

            if (!runtime.HasExistingAccount())
            {
                btnRegisterAccount.Visibility = Visibility.Visible;
                btnConnectAccount.Visibility = Visibility.Hidden;
            }
            else
            {
                btnConnectAccount.Visibility = Visibility.Visible;
                btnRegisterAccount.Visibility = Visibility.Hidden;
            }
        }
        
        private void btnRegisterAccount_Click(object sender, RoutedEventArgs e)
        {
            RegisterAccount();
        }

        private void btnConnectAccount_Click(object sender, RoutedEventArgs e)
        {
            ConnectAccount();
        }

        private void RegisterAccount()
        {
            if (runtime == null || logonStatus != LogonStatus.None)
            {
                return;
            }
            
            BackgroundWork.CreateWork(
                this,
                () => {
                    ShowStatus("Initialzing Account...");
                },
                () => {
                    logonStatus = LogonStatus.Registering;
                    runtime.Start();

                    return 0;
                },
                (taskResult) => {

                    if (taskResult.HasError)
                    {
                        MessageBox.Show("Initializing Failed: " + taskResult.Exception.Message);
                        
                        HideStatus();
                        return;
                    }

                    HideStatus();
                }
            ).Execute();
        }

        private void ConnectAccount()
        {
            if (runtime == null)
            {
                return;
            }

            BackgroundWork.CreateWork(
                this,
                () => {
                    ShowStatus("Connecting Account...");
                },
                () => {
                    logonStatus = LogonStatus.Connecting;
                    runtime.Start();

                    return 0;
                },
                (taskResult) => {

                    if (taskResult.HasError)
                    {
                        MessageBox.Show("Connecting Failed: " + taskResult.Exception.Message);

                        HideStatus();
                        return;
                    }

                    // HideStatus();
                }
            ).Execute();
        }

        private String InputPassword(String promptMessage, uint passwordSize)
        {
            if (logonStatus == LogonStatus.Registering)
            {
                PasswordWindow passwordWindow = new PasswordWindow(promptMessage, (passwordInput) =>
                {
                    userInputPassword = passwordInput;
                });
                passwordWindow.ShowDialog();

                logonStatus = LogonStatus.RegisteringConfirmPassword;
                return userInputPassword;
            }
            else if (logonStatus == LogonStatus.RegisteringConfirmPassword)
            {
                string confirmedPassword = string.Empty;
                PasswordWindow passwordWindow = new PasswordWindow(promptMessage, (passwordInput) =>
                {
                    confirmedPassword = passwordInput;
                });
                passwordWindow.ShowDialog();

                if (!confirmedPassword.Equals(userInputPassword))
                {
                    MessageBox.Show("The password is not matching the first one.");
                    logonStatus = LogonStatus.None;
                }
                else
                {
                    xDagWallet.PaswordPlain = confirmedPassword;
                    logonStatus = LogonStatus.RegisteringRandomKeys;
                }

                return confirmedPassword;
            }
            else if (logonStatus == LogonStatus.RegisteringRandomKeys)
            {
                // Dont let customer to fill the random letters, just randomly create one
                logonStatus = LogonStatus.RegisteringPending;

                string randomkey = Guid.NewGuid().ToString();
                xDagWallet.RandomKey = randomkey;

                return randomkey;
            }
            else if (logonStatus == LogonStatus.Connecting)
            {
                PasswordWindow passwordWindow = new PasswordWindow(promptMessage, (passwordInput) =>
                {
                    userInputPassword = passwordInput;
                });
                passwordWindow.ShowDialog();

                logonStatus = LogonStatus.ConnectingPending;
                return userInputPassword;
            }

            return string.Empty;
        }

        private int UpdateState(String state, String balance, String address)
        {
            /// MessageBox.Show(string.Format("Get State Update: State=[{0}], Balance=[{1}], Address=[{2}]", state, balance, address));
            

            return 0;
        }

        private void UpdateState(WalletState state)
        {
            ShowStatus(state.ToString());

            if (state == WalletState.ConnectedPool)
            {
                xDagWallet.State = state;

                WalletWindow walletWindow = new WalletWindow(xDagWallet);
                walletWindow.Show();

                this.Close();
            }
        }

        private void ShowStatus(string message)
        {
            this.lblWalletStatus.Visibility = Visibility.Visible;
            this.lblWalletStatus.Content = message;

            this.prbProgress.Visibility = Visibility.Visible;
            this.prbProgress.IsIndeterminate = true;
        }

        private void HideStatus()
        {
            this.lblWalletStatus.Visibility = Visibility.Hidden;
            this.prbProgress.Visibility = Visibility.Hidden;
            this.prbProgress.IsIndeterminate = false;
        }
    }
}
