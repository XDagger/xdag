using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using XDagNetWalletCLI;

namespace XDagNetWallet.Components
{
    public class XDagWallet : IXDagWallet
    {
        private Func<string, uint, string> promptInputPasswordFunc = null;
        private Func<string, string, string, int> updateStateFunc = null;

        private Action<WalletState> onUpdatingState = null;
        private Action<double> onUpdatingBalance = null;
        private Action<string> onUpdatingAddress = null;

        private WalletOptions walletOptions = null;

        public WalletState walletState = WalletState.None;


        public static string BalanceToString(double balance)
        {
            return String.Format("{0:0,0.00000000}", balance);
        }


        public XDagWallet(WalletOptions walletOptions = null)
        {
            if (walletOptions == null)
            {
                this.walletOptions = WalletOptions.Default();
            }
        }

        public WalletState State
        {
            get
            {
                return walletState;
            }
            set
            {
                walletState = value;
            }
        }

        public string PaswordPlain
        {
            get; set;
        }

        public string RandomKey
        {
            get; set;
        }

        public string PublicKey
        {
            get; set;
        }

        public double Balance
        {
            get; set;
        }

        public string Address
        {
            get; set;
        }

        public bool IsConnected
        {
            get
            {
                return (walletState == WalletState.ConnectedPool
                    || walletState == WalletState.ConnectedAndMining
                    || walletState == WalletState.Synchronizing
                    || walletState == WalletState.Synchronized
                    || walletState == WalletState.TransferPending);
            }
        }

        public string BalanceString
        {
            get
            {
                return BalanceToString(this.Balance);
            }
        }

        public void SetPromptInputPasswordFunction(Func<string, uint, string> f)
        {
            this.promptInputPasswordFunc = f;
        }

        public void SetUpdateStateFunction(Func<string, string, string, int> f)
        {
            this.updateStateFunc = f;
        }

        public void SetBalanceChangedAction(Action<double> f)
        {
            this.onUpdatingBalance = f;
        }

        public void SetAddressChangedAction(Action<string> f)
        {
            this.onUpdatingAddress = f;
        }

        public void SetStateChangedAction(Action<WalletState> f)
        {
            this.onUpdatingState = f;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="promptMessage"></param>
        /// <param name="passwordSize"></param>
        /// <returns></returns>
        public string OnPromptInputPassword(string promptMessage, uint passwordSize)
        {
            return this.promptInputPasswordFunc?.Invoke(promptMessage, passwordSize);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="state"></param>
        /// <param name="balance"></param>
        /// <param name="address"></param>
        /// <returns></returns>
        public int OnUpdateState(string state, string balance, string address)
        {
            WalletState newState = WalletStateConverter.ConvertFromMessage(state);

            if (newState != walletState)
            {
                this.onUpdatingState?.Invoke(newState);
                walletState = newState;
            }

            double balanceValue = 0;
            if (double.TryParse(balance, out balanceValue))
            {
                this.onUpdatingBalance?.Invoke(balanceValue);
                this.Balance = balanceValue;
            }

            if (IsValidAddress(address))
            {
                this.onUpdatingAddress?.Invoke(address);
                this.Address = address;
            }

            return this.updateStateFunc?.Invoke(state, balance, address) ?? -1;
        }

        public static bool IsValidAddress(string addressString)
        {
            if (string.IsNullOrEmpty(addressString))
            {
                return false;
            }

            if (addressString.Length != 32)
            {
                return false;
            }

            return true;
        }
    }
}
