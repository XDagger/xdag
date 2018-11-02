using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace XDagNetWallet.UI.Async
{

    public class BackgroundWorkResult<T>
    {
        public T Result
        {
            get; protected set;
        }

        public Exception Exception
        {
            get; protected set;
        }

        public BackgroundWorkResult()
        {

        }

        public static BackgroundWorkResult<T> CreateResult(T resultVal)
        {
            BackgroundWorkResult<T> result = new BackgroundWorkResult<T>();
            result.Result = resultVal;
            return result;
        }

        public static BackgroundWorkResult<T> ErrorResult(Exception ex)
        {
            BackgroundWorkResult<T> result = new BackgroundWorkResult<T>();
            result.Exception = ex;
            return result;
        }

        public bool HasError
        {
            get
            {
                return this.Exception != null;
            }
        }
    }

    public class BackgroundWorkResult : BackgroundWorkResult<int>
    {
        public BackgroundWorkResult()
        {

        }

        public BackgroundWorkResult(int result)
        {
            this.Result = result;
            this.Exception = null;
        }

        public BackgroundWorkResult(BackgroundWorkResult<int> result)
        {
            this.Result = result.Result;
            this.Exception = result.Exception;
        }

        public static BackgroundWorkResult CreateResult()
        {
            BackgroundWorkResult result = new BackgroundWorkResult();
            return result;
        }
    }
}
