using System.Text;

namespace Yuizumi.Icfpc2019
{
    public partial class Map
    {
        private Map(char[,] cells)
        {
            mCells = cells;
        }

        private readonly char[,] mCells;

        public const char Free = ' ';
        public const char Wall = '#';
        public const char Done = '.';

        public int XSize => mCells.GetLength(0);
        public int YSize => mCells.GetLength(1);

        public char this[int x, int y]
        {
            get
            {
                return InsideBoundingBox(x, y) ? mCells[x, y] : Wall;
            }
            set
            {
                if (InsideBoundingBox(x, y)) mCells[x, y] = value;
            }
        }

        public char this[Point p]
        {
            get => this[p.X, p.Y];
            set { this[p.X, p.Y] = value; }
        }

        public Map Clone() => new Map(mCells.Clone() as char[,]);

        public override string ToString()
        {
            var sb = new StringBuilder();
            for (int y = YSize - 1; y >= 0; y--)
            {
                for (int x = 0; x < XSize; x++) sb.Append(mCells[x, y]);
                if (y != 0) sb.AppendLine();
            }
            return sb.ToString();
        }

        private bool InsideBoundingBox(int x, int y)
        {
            return (0 <= x && x < XSize) && (0 <= y && y < YSize);
        }
    }
}
