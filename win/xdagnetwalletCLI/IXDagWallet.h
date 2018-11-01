#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;


namespace XDagNetWalletCLI {

	public interface class IXDagWallet
	{
	public:

		///
		/// Returning empty string indicates user cancelled
		/// 
		String^ OnPromptInputPassword(String^ promptMessage, unsigned passwordSize);

		///
		///
		///
		int OnUpdateState(String^ state, String^ balance, String^ address);

	};
};