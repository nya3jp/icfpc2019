using System;

namespace Yuizumi.Icfpc2019
{
    static class MainClass
    {
        static void Main()
        {
            try
            {
                string desc = Console.ReadLine();
                Map map = Map.Parse(desc);
                Point wrappy = Point.Parse(desc.Split('#')[1]);
                var solver = new DfsSolver(wrappy, map);
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
