using System.Text;

namespace Yuizumi.Icfpc2019
{
    public partial class Map
    {
        private Map(int cx, int cy)
        {
            mCells = new char[cx, cy];
        }

        private readonly char[,] mCells;

        public const char Free = '.';
        public const char Wall = '#';

        public int MinX => mCells.GetLowerBound(0);
        public int MaxX => mCells.GetUpperBound(0);
        public int MinY => mCells.GetLowerBound(1);
        public int MaxY => mCells.GetUpperBound(1);

        public char this[int x, int y]
        {
            get
            {
                return InsideBounding(x, y) ? mCells[x, y] : Wall;
            }
            private set
            {
                if (InsideBounding(x, y)) mCells[x, y] = value;
            }
        }

        public char this[Point p]
        {
            get => this[p.X, p.Y];
            private set { this[p.X, p.Y] = value; }
        }

        private bool InsideBounding(int x, int y)
        {
            return (MinX <= x && x <= MaxX) && (MinY <= y && y <= MaxY);
        }

        public override string ToString()
        {
            var sb = new StringBuilder();

            for (int y = MaxY; y >= MinY; y--)
            {
                for (int x = MinX; x <= MaxX; x++)
                    sb.Append(mCells[x, y] == '\0' ? ' ' : mCells[x, y]);
                if (y != MinY) sb.AppendLine();
            }

            return sb.ToString();
        }
    }
}