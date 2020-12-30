#include <WiFi.h>
#include "settings.h"

const char* ssid     = "TWANG_AP";
const char* passphrase = "12345666";

WiFiServer server(80);

char linebuf[80];
int charcount=0;

enum PAGE_TO_SEND
{
  Stats,
  Metrics
};

void ap_setup() {

    bool ret;
    
    

  /*
   * Set up an access point
   * @param ssid          Pointer to the SSID (max 63 char).
   * @param passphrase    (for WPA2 min 8 char, for open use NULL)
   * @param channel       WiFi channel number, 1 - 13.
   * @param ssid_hidden   Network cloaking (0 = broadcast SSID, 1 = hide SSID)
   */
    ret = WiFi.softAP(ssid, passphrase, 2, 0);
    server.begin();
	
    Serial.print("\r\nWiFi SSID: ");
	Serial.println(ssid);
	Serial.print("WiFi Password: ");
	Serial.println(passphrase);
	Serial.println("Web Server Address: http://192.168.4.1");
	
	}

void sendStatsPage(WiFiClient client) {
	// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
	// and a content-type so the client knows what's coming, then a blank line:
	client.println("HTTP/1.1 200 OK");
	client.println("Content-type:text/html");
	client.println();
	client.println("<html>");
	client.println("<body>");
	client.println("<h1>TWANG32 Play Stats</h1>");
	client.println("<ul>");

	client.print("<li>Games played: "); client.print(user_settings.games_played); client.println("</li>");
	if (user_settings.games_played > 0) {  // prevent divide by 0
		client.print("<li>Average score: "); client.print(user_settings.total_points / user_settings.games_played); client.println("</li>");
	}
	client.print("<li>High score: "); client.print(user_settings.high_score); client.println("</li>");
	client.print("<li>Boss kills: "); client.print(user_settings.boss_kills); client.println("</li>");

	client.print("<button onClick=\"location.href = 'http://192.168.4.1'\">Refresh</button>");

	client.print("<h2>Adjustable Settings </h2>");

	client.print("<table>");


	client.print("<tr>");
	client.printf("<td>LED Count (%d-%d)</td>", MIN_LEDS, MAX_LEDS);
	client.printf("<td><form><input type='number' name='C' value='%d' min='%d' max='%d'>", user_settings.led_count, MIN_LEDS, MAX_LEDS);
	client.print("<input type='submit'></form></td></tr>");

	client.printf("<tr><td>Brightness (%d-%d)</td>", MIN_BRIGHTNESS, MAX_BRIGHTNESS);
	client.printf("<td><form><input type='number' name='B' value='%d' min='%d' max='%d'>", user_settings.led_brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
	client.print("<input type='submit'></form></td></tr>");

	client.printf("<tr><td>Sound Volume (%d-%d)</td>", MIN_VOLUME, MAX_VOLUME);
	client.printf("<td><form><input type='number' name='S' value='%d' min='%d' max='%d'>", user_settings.audio_volume, MIN_VOLUME, MAX_VOLUME);
	client.print("<input type='submit'></form></td></tr>");

	client.printf("<tr><td>Joystick Deadzone (%d-%d)</td>", MIN_JOYSTICK_DEADZONE, MAX_JOYSTICK_DEADZONE);
	client.printf("<td><form><input type='number' name='D' value='%d' min='%d' max='%d'>", user_settings.joystick_deadzone, MIN_JOYSTICK_DEADZONE, MAX_JOYSTICK_DEADZONE);
	client.print("<input type='submit'></form></td></tr>");

	client.printf("<tr><td>Attack Sensitivity (%d-%d)</td>", MIN_ATTACK_THRESHOLD, MAX_ATTACK_THRESHOLD);
	client.printf("<td><form><input type='number' name='A' value='%d' min='%d' max='%d'>", user_settings.attack_threshold, MIN_ATTACK_THRESHOLD, MAX_ATTACK_THRESHOLD);
	client.print("<input type='submit'></form></td></tr>");

	client.printf("<tr><td>Lives Per Level (%d-%d)</td>", MIN_LIVES_PER_LEVEL, MAX_LIVES_PER_LEVEL);
	client.printf("<td><form><input type='number' name='L' value='%d' min='%d' max='%d'>", user_settings.lives_per_level, MIN_LIVES_PER_LEVEL, MAX_LIVES_PER_LEVEL);
	client.print("<input type='submit'></form></td></tr>");

	client.print("</table>");

#ifdef ENABLE_PROMETHEUS_METRICS_ENDPOINT
	client.println("<ul><li><a href=\"/metrics\">Metrics</a></li></ul>");
#endif // ENABLE_PROMETHEUS_METRICS_ENDPOINT

	client.println("</body>");
	client.println("</html>");
	client.println();

}

#ifdef ENABLE_PROMETHEUS_METRICS_ENDPOINT
// We need to use print() here since println() prints newlines as CR/LF, which
// Prometheus cannot handle.
#define __prom_metric(metric_name, metric_description, value) \
	client.print("# HELP " metric_name " " metric_description "\n");   \
	client.print("# TYPE " metric_name " gauge\n");                    \
	client.print(metric_name " ");                                     \
	client.print(value);                                               \
	client.print("\n");

static void sendMetricsPage(WiFiClient client)
{
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/plain; charset=utf-8");
	client.println("Server: twang_exporter");
	client.println();
	__prom_metric("twang_games_played", "Number of games played", user_settings.games_played);
	__prom_metric("twang_total_points", "Total points", user_settings.total_points);
	__prom_metric("twang_high_score", "High score", user_settings.high_score);
	__prom_metric("twang_boss_kills", "Boss kills", user_settings.boss_kills);
}

#undef __prom_metric
#endif // ENABLE_PROMETHEUS_METRICS_ENDPOINT

void ap_client_check(){
	int cnt;
	bool newconn=false;
	int stat;
	WiFiClient client = server.available();   // listen for incoming clients

	bool currentLineIsBlank = true;
	PAGE_TO_SEND page_to_send = Stats;

	while (client.connected()) {
		if (client.available()) {
			char c = client.read();
			linebuf[charcount]=c;
			if (charcount<sizeof(linebuf)-1)
				charcount++;

			if (c == '\n' && currentLineIsBlank)
			{
				switch (page_to_send)
				{
				case Stats:
					sendStatsPage(client);
					break;
#ifdef ENABLE_PROMETHEUS_METRICS_ENDPOINT
				case Metrics:
					sendMetricsPage(client);
					break;
#endif // ENABLE_PROMETHEUS_METRICS_ENDPOINT
				}
				break;
			}
			if (c == '\n') {
				// you're starting a new line
				currentLineIsBlank = true;

				if (strstr(linebuf,"GET /?") > 0)
				{
					String line = String(linebuf);

					int start = line.indexOf('=', 0);

					char paramCode = line.charAt(start - 1);

					int finish = line.indexOf('H', start+1)-1;
					String val = line.substring(start+1, finish);
					// if it is not numeric, it will convert to 0.
					// The the change_setting function will make sure the range is OK

					change_setting(paramCode, val.toInt());

					page_to_send = Stats;
				}
#ifdef ENABLE_PROMETHEUS_METRICS_ENDPOINT
				else if (strstr(linebuf, "GET /metrics"))
				{
					page_to_send = Metrics;
				}
#endif // ENABLE_PROMETHEUS_METRICS_ENDPOINT

				// you're starting a new line
				currentLineIsBlank = true;
				memset(linebuf,0,sizeof(linebuf));
				charcount=0;
			} else if (c != '\r') {
				// you've gotten a character on the current line
				currentLineIsBlank = false;
			}
		}
	}
}
