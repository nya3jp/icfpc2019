using System;

namespace Yuizumi.Icfpc2019
{
    static class MainClass
    {
        static void Main()
        {
            try
            {
                State state = State.Parse(Console.ReadLine());
                var solver = new DfsSolver(state);
                Console.WriteLine(solver.Solve());
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e);
                Environment.Exit(1);
            }
        }
    }
}
