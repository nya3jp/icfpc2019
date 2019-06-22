using System;

namespace Yuizumi.Icfpc2019
{
    public struct Delta
    {
        public Delta(int x, int y)
        {
            X = x;
            Y = y;
        }

        public int X { get; }
        public int Y { get; }

        public override string ToString() => $"<{X},{Y}>";
    }
}
