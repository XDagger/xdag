#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

#include "IXDagWallet.h"

#pragma unmanaged

// external stdcall methods connects with xdag_runtime.h
typedef int(__stdcall * InputPasswordStd)(const char*, char*, unsigned);
typedef int(__stdcall * ShowStateStd)(const char*, const char*, const char*);

#pragma managed


namespace XDagNetWalletCLI {

	public ref class XDagRuntime
	{
	public:

		XDagRuntime(IXDagWallet^ wallet);
		~XDagRuntime();

		// Delegate method to pass to unmanaged code
		[UnmanagedFunctionPointer(CallingConvention::StdCall)]
		delegate int InputPasswordDelegate(const char*, char*, unsigned);
		[UnmanagedFunctionPointer(CallingConvention::StdCall)]
		delegate int ShowStateDelegate(const char*, const char*, const char*);


		// Interop methods to translate the parameters from unmanaged code to managed code
		int InputPassword(const char *prompt, char *buf, unsigned size);
		int ShowState(const char *state, const char *balance, const char *address);

		void Start();

		bool HasExistingAccount();

		void DoTesting();

		
	private:

		GCHandle gch;

		IXDagWallet^ xdagWallet;

		String^ ConvertFromConstChar(const char* str);

		const char* ConvertFromString(String^ str);
	};

}
