using System.Collections.Generic;

namespace Yuizumi.Icfpc2019
{
    public class State
    {
        private State(Point start, Map map)
        {
            mWrappies = new List<Wrappy>() {
                new Wrappy(start),
            };
            Wrappies = mWrappies.AsReadOnly();

            Map = map;
        }

        private readonly List<Wrappy> mWrappies;

        public IReadOnlyList<Wrappy> Wrappies { get; }
        public Map Map { get; }

        public static State Parse(string desc)
        {
            return new State(Point.Parse(desc.Split("#")[1]), Map.Parse(desc));
        }
    }
}
