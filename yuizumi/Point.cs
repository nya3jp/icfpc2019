using System;

namespace Yuizumi.Icfpc2019
{
    public struct Point
    {
        public Point(int x, int y)
        {
            X = x;
            Y = y;
        }

        public int X { get; }
        public int Y { get; }

        public static Point Parse(string value)
        {
            string[] xy = value.Substring(1, value.Length - 2).Split(',');
            return new Point(Int32.Parse(xy[0]), Int32.Parse(xy[1]));
        }

        public override string ToString() => $"({X},{Y})";
    }
}
