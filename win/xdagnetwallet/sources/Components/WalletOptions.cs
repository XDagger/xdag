using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace XDagNetWallet.Components
{
    public class WalletOptions
    {
        public bool RpcEnabled
        {
            get; set;
        }

        public int RpcPort
        {
            get; set;
        }

        public string SpecifiedPoolAddress
        {
            get; set;
        }

        public bool IsTestNet
        {
            get; set;
        }

        public bool DisableMining
        {
            get; set;
        }

        public WalletOptions()
        {

        }

        public static WalletOptions Default()
        {
            WalletOptions options = new WalletOptions();

            options.IsTestNet = false;
            options.DisableMining = true;
            options.RpcEnabled = false;

            return options;
        }

        public static WalletOptions TestNet()
        {
            WalletOptions options = Default();
            options.IsTestNet = true;

            return options;
        }

    }
}
