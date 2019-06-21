using System;
using System.Collections.Generic;
using System.Linq;

namespace Yuizumi.Icfpc2019
{
    public partial class Map
    {
        public static Map Parse(string desc)
        {
            string[] chunks = desc.Split('#');

            IReadOnlyList<Point> outer = ParsePolygon(chunks[0]);

            int cx = 0, cy = 0;
            foreach (Point p in outer)
            {
                cx = Math.Max(cx, p.X);
                cy = Math.Max(cy, p.Y);
            }
            var map = new Map(cx, cy);

            map.DrawWall(outer, 1, 0);

            if (chunks[2] != "")
            {
                foreach (string subdesc in chunks[2].Split(';'))
                {
                    map.DrawWall(ParsePolygon(subdesc), 0, 1);
                }
            }

            Point wrappy = Point.Parse(chunks[1]);
            map.FillFree(wrappy.X, wrappy.Y);

            map.FillWall();

            if (chunks[3] != "")
            {
                foreach (string subdesc in chunks[3].Split(';'))
                {
                    map[Point.Parse(subdesc.Substring(1))] = subdesc[0];
                }
            }

            return map;
        }

        private static IReadOnlyList<Point> ParsePolygon(string desc)
        {
            string[] coords = desc.Substring(1, desc.Length - 2).Split("),(");

            var polygon = new List<Point>(coords.Length);
            for (int i = 0; i < coords.Length; i++)
            {
                string[] xy = coords[i].Split(',');
                polygon.Add(new Point(Int32.Parse(xy[0]), Int32.Parse(xy[1])));
            }

            return polygon;
        }

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

        private void FillWall()
        {
            for (int y = MinY; y <= MaxY; y++)
            {
                for (int x = MinX; x <= MaxX; x++)
                {
                    if (this[x, y] == '\0') this[x, y] = Wall;
                }
            }
        }
    }
}
