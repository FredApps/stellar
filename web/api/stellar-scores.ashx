<%@ WebHandler Language="C#" Class="StellarScoresHandler" %>

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Web;
using System.Web.Script.Serialization;

public class StellarScoresHandler : IHttpHandler
{
    private static readonly object Gate = new object();
    private static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 1024 * 1024 };

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
                WriteJson(context, new Dictionary<string, object> { { "ok", true }, { "scores", LoadScores() } });
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

        var entry = new ScoreEntry
        {
            name = CleanName(GetString(payload, "name")),
            score = Clamp(GetInt(payload, "score"), 0, 999999999),
            wave = Clamp(GetInt(payload, "wave"), 0, 999),
            maxCombo = Clamp(GetInt(payload, "maxCombo"), 0, 9999),
            bosses = Clamp(GetInt(payload, "bosses"), 0, 999),
            at = DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ")
        };

        if (entry.score <= 0)
        {
            WriteError(context, 400, "Score must be positive.");
            return;
        }

        List<ScoreEntry> scores;
        lock (Gate)
        {
            scores = LoadScoresUnlocked();
            scores.Add(entry);
            scores = scores
                .OrderByDescending(s => s.score)
                .ThenByDescending(s => s.wave)
                .ThenByDescending(s => s.maxCombo)
                .Take(10)
                .ToList();
            SaveScoresUnlocked(scores);
        }

        WriteJson(context, new Dictionary<string, object> { { "ok", true }, { "scores", scores } });
    }

    private static List<ScoreEntry> LoadScores()
    {
        lock (Gate) return LoadScoresUnlocked();
    }

    private static List<ScoreEntry> LoadScoresUnlocked()
    {
        var path = ScorePath();
        if (!File.Exists(path)) return DefaultScores();
        try
        {
            var scores = Json.Deserialize<List<ScoreEntry>>(File.ReadAllText(path));
            if (scores == null) return DefaultScores();
            return scores
                .Where(s => s != null && s.score > 0)
                .Select(s => new ScoreEntry {
                    name = CleanName(s.name),
                    score = Clamp(s.score, 0, 999999999),
                    wave = Clamp(s.wave, 0, 999),
                    maxCombo = Clamp(s.maxCombo, 0, 9999),
                    bosses = Clamp(s.bosses, 0, 999),
                    at = String.IsNullOrWhiteSpace(s.at) ? "" : s.at
                })
                .OrderByDescending(s => s.score)
                .ThenByDescending(s => s.wave)
                .ThenByDescending(s => s.maxCombo)
                .Take(10)
                .ToList();
        }
        catch
        {
            return DefaultScores();
        }
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
        var mapped = HttpContext.Current.Server.MapPath("~/App_Data/stellar-assault-scores.json");
        return mapped ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "App_Data", "stellar-assault-scores.json");
    }

    private static List<ScoreEntry> DefaultScores()
    {
        var names = new[] { "ACE", "NOVA", "COMET", "ORION", "VEGA", "PULSAR", "ROOKIE", "CADET", "DRIFT", "PIXEL" };
        var list = new List<ScoreEntry>();
        for (var i = 0; i < 10; i++)
        {
            list.Add(new ScoreEntry { name = names[i], score = (10 - i) * 1000, wave = 1, maxCombo = 0, bosses = 0, at = "" });
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
    }
}
