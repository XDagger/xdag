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

        private WalletOptions walletOptions = null;

        public WalletState walletState = WalletState.None;

        public XDagWallet(WalletOptions walletOptions = null)
        {
            if (walletOptions == null)
            {
                this.walletOptions = WalletOptions.Default();
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

        public void SetPromptInputPasswordFunction(Func<string, uint, string> f)
        {
            this.promptInputPasswordFunc = f;
        }

        public void SetUpdateStateFunction(Func<string, string, string, int> f)
        {
            this.updateStateFunc = f;
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
            return this.updateStateFunc?.Invoke(state, balance, address) ?? -1;
        }
    }
}
