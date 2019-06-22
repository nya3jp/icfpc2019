using System.Collections.Generic;

namespace Yuizumi.Icfpc2019
{
    public class Wrappy
    {
        public Wrappy(Point point)
        {
            Point = point;

            mManips = new List<Delta>() {
                new Delta(+1, -1),
                new Delta(+1,  0),
                new Delta(+1, +1),
            };
            Manips = mManips.AsReadOnly();
        }

        private readonly List<Delta> mManips;

        public Point Point { get; internal set; }
        public IReadOnlyCollection<Delta> Manips { get; }

        public int X => Point.X;
        public int Y => Point.Y;

        internal void AddManip(int dx, int dy)
            => mManips.Add(new Delta(dx, dy));

        internal void Rotate(int ccw)
        {
            for (int i = 0; i < mManips.Count; i++)
            {
                mManips[i] = new Delta(-ccw * mManips[i].Y, +ccw * mManips[i].X);
            }
        }
    }
}
