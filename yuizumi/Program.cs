using System;

namespace Yuizumi.Icfpc2019
{
    static class MainClass
    {
        static void Main()
        {
            try
            {
                Map map = Map.Parse(Console.ReadLine());
                Console.WriteLine(map);
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e);
                Environment.Exit(1);
            }
        }
    }
}
