using System;
using System.Collections.Generic;
using System.Linq;

namespace Yuizumi.Icfpc2019
{
    using static StringSplitOptions;

    public partial class Map
    {
        public static Map Parse(string desc)
        {
            string[] chunks = desc.Split('#');

            IReadOnlyList<Point> outer = ParsePolygon(chunks[0]);
            int xmax = 0, ymax = 0;
            foreach (Point p in outer)
            {
                xmax = Math.Max(xmax, p.X);
                ymax = Math.Max(ymax, p.Y);
            }
            var map = new Map(new char[xmax, ymax]);

            map.DrawOuterWall(outer);

            foreach (string subdesc in Split(chunks[2], ';'))
                map.DrawInnerWall(ParsePolygon(subdesc));

            Point start = Point.Parse(chunks[1]);
            map.FillFree(start.X, start.Y);

            for (int y = 0; y < ymax; y++)
            for (int x = 0; x < xmax; x++)
            {
                if (map.mCells[x, y] == '\0') map.mCells[x, y] = Wall;
            }

            foreach (string subdesc in Split(chunks[3], ';'))
            {
                char booster = subdesc[0];
                map[Point.Parse(subdesc.Substring(1))] = booster;
            }

            return map;
        }

        private static string[] Split(string value, params char[] split)
            => value.Split(split, RemoveEmptyEntries);

        private static IReadOnlyList<Point> ParsePolygon(string desc)
        {
            return desc.Substring(1, desc.Length - 2).Split("),(")
                .Select(s => s.Split(','))
                .Select(xy => new Point(Int32.Parse(xy[0]), Int32.Parse(xy[1])))
                .ToList().AsReadOnly();
        }

        private void DrawOuterWall(IReadOnlyList<Point> polygon)
            => DrawWall(polygon, 1, 0);

        private void DrawInnerWall(IReadOnlyList<Point> polygon)
            => DrawWall(polygon, 0, 1);

        private void DrawWall(IReadOnlyList<Point> polygon, int outer, int inner)
        {
            for (int i = 0; i < polygon.Count; i++)
            {
                Point p = polygon[(i + 0) % polygon.Count];
                Point q = polygon[(i + 1) % polygon.Count];

                if (p.X < q.X)
                {
                    int y = p.Y - outer;
                    for (int x = p.X; x < q.X; x++) this[x, y] = Wall;
                }
                else if (p.Y < q.Y)
                {
                    int x = p.X - inner;
                    for (int y = p.Y; y < q.Y; y++) this[x, y] = Wall;
                }
                else if (p.X > q.X)
                {
                    int y = p.Y - inner;
                    for (int x = q.X; x < p.X; x++) this[x, y] = Wall;
                }
                else if (p.Y > q.Y)
                {
                    int x = p.X - outer;
                    for (int y = q.Y; y < p.Y; y++) this[x, y] = Wall;
                }
            }
        }

        private void FillFree(int x, int y)
        {
            this[x, y] = Free;
            if (this[x - 1, y] == '\0') FillFree(x - 1, y);
            if (this[x + 1, y] == '\0') FillFree(x + 1, y);
            if (this[x, y - 1] == '\0') FillFree(x, y - 1);
            if (this[x, y + 1] == '\0') FillFree(x, y + 1);
        }
    }
}
