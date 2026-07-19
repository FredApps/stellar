<%@ WebHandler Language="C#" Class="AyrienScoresHandler" %>

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Web;
using System.Web.Script.Serialization;

public class AyrienScoresHandler : IHttpHandler
{
    private static readonly object Gate = new object();
    private static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 1024 * 1024 };
    private const string Desktop = "desktop";
    private const string Mobile = "mobile";

    public bool IsReusable { get { return false; } }

    public void ProcessRequest(HttpContext context)
    {
        context.Response.TrySkipIisCustomErrors = true;
        context.Response.Cache.SetCacheability(HttpCacheability.NoCache);
        context.Response.Cache.SetNoStore();

        try
        {
            if (String.Equals(context.Request.HttpMethod, "POST", StringComparison.OrdinalIgnoreCase))
            {
                HandlePost(context);
                return;
            }
            if (String.Equals(context.Request.HttpMethod, "GET", StringComparison.OrdinalIgnoreCase))
            {
                WriteJson(context, ScoreResponse(LoadScores()));
                return;
            }
            WriteError(context, 405, "Method not allowed.");
        }
        catch (Exception)
        {
            WriteError(context, 500, "Internal server error.");
        }
    }

    private static void HandlePost(HttpContext context)
    {
        Dictionary<string, object> payload;
        if (context.Request.ContentLength > 4096)
        {
            WriteError(context, 413, "Request body too large.");
            return;
        }
        using (var reader = new StreamReader(context.Request.InputStream))
        {
            var body = reader.ReadToEnd();
            if (body.Length > 4096)
            {
                WriteError(context, 413, "Request body too large.");
                return;
            }
            try
            {
                payload = String.IsNullOrWhiteSpace(body)
                    ? new Dictionary<string, object>()
                    : Json.Deserialize<Dictionary<string, object>>(body);
            }
            catch (ArgumentException)
            {
                WriteError(context, 400, "Invalid JSON body.");
                return;
            }
            catch (InvalidOperationException)
            {
                WriteError(context, 400, "Invalid JSON body.");
                return;
            }
        }
        if (payload == null) payload = new Dictionary<string, object>();

        var platform = GetString(payload, "platform").Trim().ToLowerInvariant();
        if (platform.Length == 0) platform = Desktop;
        if (platform != Desktop && platform != Mobile)
        {
            WriteError(context, 400, "Platform must be desktop or mobile.");
            return;
        }

        var entry = new ScoreEntry
        {
            name = CleanName(GetString(payload, "name")),
            score = Clamp(GetInt(payload, "score"), 0, 999999999),
            wave = Clamp(GetInt(payload, "wave"), 0, 999),
            maxCombo = Clamp(GetInt(payload, "maxCombo"), 0, 9999),
            bosses = Clamp(GetInt(payload, "bosses"), 0, 999),
            at = DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ"),
            platform = platform
        };

        if (entry.score <= 0)
        {
            WriteError(context, 400, "Score must be positive.");
            return;
        }

        ScoreBoards boards;
        bool personalBestImproved;
        lock (Gate)
        {
            bool changed;
            var scores = LoadScoresUnlocked(out changed);
            var previous = scores.FirstOrDefault(s => s.platform == entry.platform && s.name == entry.name);
            personalBestImproved = previous == null || IsBetter(entry, previous);
            scores.Add(entry);
            scores = NormalizeScores(scores);
            SaveScoresUnlocked(scores);
            boards = SplitScores(scores);
        }

        var response = ScoreResponse(boards);
        response.Add("personalBestImproved", personalBestImproved);
        WriteJson(context, response);
    }

    private static ScoreBoards LoadScores()
    {
        lock (Gate)
        {
            bool changed;
            var scores = LoadScoresUnlocked(out changed);
            if (changed) SaveScoresUnlocked(scores);
            return SplitScores(scores);
        }
    }

    private static List<ScoreEntry> LoadScoresUnlocked(out bool changed)
    {
        var path = ScorePath();
        if (!File.Exists(path)) path = LegacyScorePath();
        if (!File.Exists(path)) { changed = false; return DefaultScores(); }
        try
        {
            var scores = Json.Deserialize<List<ScoreEntry>>(File.ReadAllText(path));
            if (scores == null) { changed = false; return DefaultScores(); }
            var normalized = NormalizeScores(scores);
            changed = Json.Serialize(scores) != Json.Serialize(normalized);
            return normalized;
        }
        catch
        {
            changed = false;
            return DefaultScores();
        }
    }

    private static List<ScoreEntry> NormalizeScores(IEnumerable<ScoreEntry> source)
    {
        var cleaned = source
            .Where(s => s != null && s.score > 0)
            .Select(s => new ScoreEntry {
                name = CleanName(s.name),
                score = Clamp(s.score, 0, 999999999),
                wave = Clamp(s.wave, 0, 999),
                maxCombo = Clamp(s.maxCombo, 0, 9999),
                bosses = Clamp(s.bosses, 0, 999),
                at = String.IsNullOrWhiteSpace(s.at) ? "" : s.at,
                platform = CleanPlatform(s.platform, s.name)
            })
            .GroupBy(s => s.platform + "\n" + s.name)
            .Select(g => Rank(g).First())
            .ToList();
        return Rank(cleaned.Where(s => s.platform == Desktop)).Take(10)
            .Concat(Rank(cleaned.Where(s => s.platform == Mobile)).Take(10))
            .ToList();
    }

    private static IOrderedEnumerable<ScoreEntry> Rank(IEnumerable<ScoreEntry> scores)
    {
        return scores.OrderByDescending(s => s.score)
            .ThenByDescending(s => s.wave)
            .ThenByDescending(s => s.maxCombo)
            .ThenByDescending(s => s.bosses)
            .ThenByDescending(s => s.at, StringComparer.Ordinal);
    }

    private static bool IsBetter(ScoreEntry candidate, ScoreEntry previous)
    {
        if (candidate.score != previous.score) return candidate.score > previous.score;
        if (candidate.wave != previous.wave) return candidate.wave > previous.wave;
        if (candidate.maxCombo != previous.maxCombo) return candidate.maxCombo > previous.maxCombo;
        if (candidate.bosses != previous.bosses) return candidate.bosses > previous.bosses;
        return String.CompareOrdinal(candidate.at, previous.at) > 0;
    }

    private static string CleanPlatform(string value, string name)
    {
        value = (value ?? "").Trim().ToLowerInvariant();
        if (value == Mobile) return Mobile;
        if (value == Desktop) return Desktop;
        return CleanName(name) == "NEF" ? Mobile : Desktop;
    }

    private static ScoreBoards SplitScores(IEnumerable<ScoreEntry> scores)
    {
        return new ScoreBoards {
            desktop = Rank(scores.Where(s => s.platform == Desktop)).Take(10).ToList(),
            mobile = Rank(scores.Where(s => s.platform == Mobile)).Take(10).ToList()
        };
    }

    private static Dictionary<string, object> ScoreResponse(ScoreBoards boards)
    {
        return new Dictionary<string, object> {
            { "ok", true },
            { "desktopScores", boards.desktop },
            { "mobileScores", boards.mobile },
            { "scores", boards.desktop }
        };
    }

    private static void SaveScoresUnlocked(List<ScoreEntry> scores)
    {
        var path = ScorePath();
        var dir = Path.GetDirectoryName(path);
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        var tmp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        File.WriteAllText(tmp, Json.Serialize(scores));
        if (File.Exists(path)) File.Replace(tmp, path, null);
        else File.Move(tmp, path);
    }

    private static string ScorePath()
    {
        var mapped = HttpContext.Current.Server.MapPath("~/App_Data/ayrien-assault-scores.json");
        return mapped ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "App_Data", "ayrien-assault-scores.json");
    }

    private static string LegacyScorePath()
    {
        var mapped = HttpContext.Current.Server.MapPath("~/App_Data/stellar-assault-scores.json");
        return mapped ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "App_Data", "stellar-assault-scores.json");
    }

    private static List<ScoreEntry> DefaultScores()
    {
        var names = new[] { "ACE", "NOVA", "COMET", "ORION", "VEGA", "PULSAR", "ROOKIE", "CADET", "DRIFT", "PIXEL" };
        var list = new List<ScoreEntry>();
        for (var i = 0; i < 10; i++)
        {
            list.Add(new ScoreEntry { name = names[i], score = (10 - i) * 1000, wave = 1, maxCombo = 0, bosses = 0, at = "", platform = Desktop });
            list.Add(new ScoreEntry { name = names[i], score = (10 - i) * 1000, wave = 1, maxCombo = 0, bosses = 0, at = "", platform = Mobile });
        }
        return list;
    }

    private static string CleanName(string value)
    {
        value = (value ?? "PLAYER").ToUpperInvariant();
        value = Regex.Replace(value, @"[^A-Z0-9 $]", "");
        value = value.Trim();
        if (value.Length == 0) value = "PLAYER";
        return value.Length > 8 ? value.Substring(0, 8) : value;
    }

    private static string GetString(Dictionary<string, object> payload, string key)
    {
        return payload.ContainsKey(key) && payload[key] != null ? Convert.ToString(payload[key]) : "";
    }

    private static int GetInt(Dictionary<string, object> payload, string key)
    {
        int parsed;
        return Int32.TryParse(GetString(payload, key), out parsed) ? parsed : 0;
    }

    private static int Clamp(int value, int lo, int hi)
    {
        return value < lo ? lo : value > hi ? hi : value;
    }

    private static void WriteJson(HttpContext context, object value)
    {
        context.Response.ContentType = "application/json";
        context.Response.Write(Json.Serialize(value));
    }

    private static void WriteError(HttpContext context, int status, string message)
    {
        context.Response.StatusCode = status;
        WriteJson(context, new Dictionary<string, object> { { "ok", false }, { "error", message } });
    }

    public class ScoreEntry
    {
        public string name { get; set; }
        public int score { get; set; }
        public int wave { get; set; }
        public int maxCombo { get; set; }
        public int bosses { get; set; }
        public string at { get; set; }
        public string platform { get; set; }
    }

    public class ScoreBoards
    {
        public List<ScoreEntry> desktop { get; set; }
        public List<ScoreEntry> mobile { get; set; }
    }
}
