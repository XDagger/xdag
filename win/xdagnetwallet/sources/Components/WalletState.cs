using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace XDagNetWallet.Components
{
    public enum WalletState
    {
        None = 0,               // Not an account
        Registering = 10,       // Creating a new wallet and waiting for response

        Idle = 20,              // Not connected to network

        LoadingBlocks = 30,     // Loading blocks from the local storage.
        Stopped = 40,           // Blocks loaded. Waiting for 'run' command.

        ConnectingNetwork = 50, // Trying to connect to the network.
        ConnectedNetwork = 55,  // Connected to the network

        ConnectingPool = 60,    // Trying to connect to the pool.

        ConnectedPool = 65,     // Connected to the pool. No mining.
        ConnectedAndMining = 67,// Connected to the pool. Mining on. Normal operation.

        Synchronizing = 70,     // Synchronizing with the network
        Synchronized = 75,     // Synchronized with the network

        TransferPending = 80,   // Waiting for transfer to complete.

        ResetingEngine = 90,    // The local storage is corrupted. Resetting blocks engine.
    }
}
