using System.Text;

namespace Yuizumi.Icfpc2019
{
    public class DfsSolver
    {
        public DfsSolver(Point wrappy, Map map)
        {
            mWrappy = wrappy;
            mMap = map;

            for (int y = 0; y <= mMap.YSize; y++)
            for (int x = 0; x <= mMap.XSize; x++)
                if (mMap[x, y] != Map.Wall) ++mCount;
        }

        private const char done = '*';

        private readonly Point mWrappy;
        private readonly Map mMap;

        private readonly int mCount = 0;

        public string Solve()
        {
            var sb = new StringBuilder();
            Go(mWrappy.X, mWrappy.Y, sb, mCount);
            return sb.ToString();
        }

        private int Go(int x, int y, StringBuilder sb, int count)
        {
            mMap[x, y] = done;

            if (++count == mCount)
                return count;

            if (mMap[x - 1, y] != done && mMap[x - 1, y] != Map.Wall)
            {
                sb.Append('A');
                if ((count = Go(x - 1, y, sb, count)) == mCount) return count;
                sb.Append('D');
            }
            if (mMap[x + 1, y] != done && mMap[x + 1, y] != Map.Wall)
            {
                sb.Append('D');
                if ((count = Go(x + 1, y, sb, count)) == mCount) return count;
                sb.Append('A');
            }
            if (mMap[x, y - 1] != done && mMap[x, y - 1] != Map.Wall)
            {
                sb.Append('S');
                if ((count = Go(x, y - 1, sb, count)) == mCount) return count;
                sb.Append('W');
            }
            if (mMap[x, y + 1] != done && mMap[x, y + 1] != Map.Wall)
            {
                sb.Append('W');
                if ((count = Go(x, y + 1, sb, count)) == mCount) return count;
                sb.Append('S');
            }

            return count;
        }
    }
}
