using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace XDagNetWallet.UI.Async
{
    public class BackgroundWork : BackgroundWork<int>
    {
        public static BackgroundWork CreateWork(Window window, Action begin, Action func, Action<BackgroundWorkResult> end)
        {
            BackgroundWork work = new BackgroundWork();

            work.Window = window;
            work.BeginAction = begin;
            work.WorkFunction = new Func<int>(() => { func(); return 0; });
            work.EndAction = new Action<BackgroundWorkResult<int>>(
                (result) => { BackgroundWorkResult newResult = new BackgroundWorkResult(result); end(newResult); });

            return work;
        }
    }

    public class BackgroundWork<T>
    {
        protected Window Window = null;

        protected Action BeginAction = null;

        protected Func<T> WorkFunction = null;

        protected Action<BackgroundWorkResult<T>> EndAction = null;

        public static BackgroundWork<T> CreateWork(Window window, Action begin, Func<T> func, Action<BackgroundWorkResult<T>> end)
        {
            BackgroundWork<T> work = new BackgroundWork<T>();

            work.Window = window;
            work.BeginAction = begin;
            work.WorkFunction = func;
            work.EndAction = end;

            return work;
        }

        /// <summary>
        /// Main function to execute the work
        /// </summary>
        public void Execute()
        {
            if (WorkFunction == null)
            {
                throw new ArgumentNullException("Work Function cannot be null.");
            }

            BeginAction?.Invoke();

            Func<BackgroundWorkResult<T>> functionWrapper = (() =>
            {
                try
                {
                    T resultValue = WorkFunction();
                    return BackgroundWorkResult<T>.CreateResult(resultValue);
                }
                catch (Exception ex)
                {
                    return BackgroundWorkResult<T>.ErrorResult(ex);
                }
            }
            );

            if (EndAction == null)
            {
                Task.Factory.StartNew(functionWrapper);
                return;
            }

            Action<Task<BackgroundWorkResult<T>>> endActionWrapper = ((taskResult) =>
                this.Window.Dispatcher.Invoke(new Action(() => this.EndAction(taskResult.Result)))
            );

            try
            {
                Task.Factory.StartNew(functionWrapper).ContinueWith(endActionWrapper);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException("Execute BackgroundWork Failed.", ex);
            }
        }
    }
}
