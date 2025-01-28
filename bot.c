#include <WiFi.h>
#include <GyverHTTP.h>
#include <GSON.h>
#include <FastBot2.h>

#define FB_NO_FILE          // убрать поддержку работы с файлами
#define WIFI_SSID "wifi ssid"
#define WIFI_PASS "wifi pass"
#define BOT_TOKEN "telegram token"
#define CHAT_ID "1234678910" // первый id админа
#define CHAT_ID_L "1251201935" // второй id админа
#define OPENWEATHER_API_KEY "api key"


FastBot2 bot;

// Создаём WiFi клиент для HTTPS
WiFiClientSecure client;

// custom header collector
class Collector : public ghttp::HeadersCollector {
   public:
    void header(Text& name, Text& value) {
        Serial.print(name);
        Serial.print(": ");
        Serial.println(value);
    }
};

// GyverHTTP клиент для работы с OpenWeatherMap API
ghttp::Client http(client, "api.openweathermap.org", 443);

// Функция для получения погоды
String getWeather(String CITY) {
    client.setInsecure();
    String weatherData;

    // Формируем URL запроса
    String url = "/data/2.5/weather?q=" + CITY + "&appid=" + OPENWEATHER_API_KEY + "&units=metric";

    // Выполняем запрос
    http.request(url, "GET");

    // Ожидаем завершения запроса
    while (!http.available()) {
        http.tick();
    }


    // Обработка ответа
    Collector collector;
    ghttp::Client::Response resp = http.getResponse(&collector);

    if (resp.code() == 200) {  // HTTP 200 OK
        // Считываем тело ответа в строку
        String responseBody;
        while (resp.body().available()) {
            responseBody += (char)resp.body().read();
        }

        // Парсим JSON через GSON
        gson::Parser parser(10);
        parser.parse(responseBody);

        if (!parser.has(F("main")) || !parser[F("main")].has(F("temp")) ||
            !parser.has(F("weather")) || !parser[F("weather")][0].has(F("description"))) {
            weatherData = "Error parsing weather data.";
            return weatherData;
        }

        // Извлекаем температуру как float и преобразуем в int
        float tempFloat = (float)parser[F("main")][F("temp")];
        int temp = (int)tempFloat;  // Округляем до целого числа

        String description = (String)parser[F("weather")][0][F("description")];

        // Формируем итоговую строку с погодой
        weatherData = "<b>City:</b> " + CITY +
                      "\n<b>Temperature:</b> " + String(temp) + "°C" +
                      "\n<b>Description:</b> " + description;
    } else {
        weatherData = "Error fetching weather: HTTP code " + String(resp.code());
    }

    return weatherData;
}

void updateh(fb::Update& u) {
  Text user_id = u.message().from().id();
  if (user_id != CHAT_ID && user_id != CHAT_ID_L) return;

  Text chat_id = u.message().chat().id();
  fb::Message msg;
  msg.mode = fb::Message::Mode::HTML;
  msg.chatID = chat_id;

  // CallbackQuery - ответы не кнопки
  if (u.isQuery()) {
    bot.answerCallbackQuery(u.query().id(), "✅ Успешно");
    // пример с разбором callback query через хэш
    switch (u.query().data().hash()) {
      case SH("test"): {
        msg.text ="Test answer";
        bot.sendMessage(msg);
        } break;
      
      default: return;
    }
  }
  // текстовые сообщения
  if (u.isMessage()) {
      fb::Result setTyping(Value chat_id, bool wait = true);

      String text = u.message().text().decodeUnicode();
      if (text == "/start") {
          fb::MyCommands commands(
            "start;menu;help;settings",
            "Запуск бота 🚀;Главное меню ☰;Помощь ❓;Настройки ⚙️"
          );
          bot.setMyCommands(commands);

          msg.text = F(
              "<b>Помощь:</b>\r\n"
              "/start - Запуск бота 🚀\r\n"
              "/quote - Фраза дня\r\n"
              "/temp - Погода \r\n"
              "/menu -  главное меню ⭐\r\n"
              "/send_inline_menu - инлайн-меню\r\n"
          );
          msg.removeMenu();
          bot.sendMessage(msg);
      } else if (text == "/help") {
          msg.text = F(
              "<b>Помощь:</b>\r\n"
              "/start - Запуск бота 🚀\r\n"
              "/quote - Фраза дня\r\n"
              "/menu -  главное меню ⭐\r\n"
              "/send_inline_menu - инлайн-меню\r\n"
          );
          msg.removeMenu();
          bot.sendMessage(msg);
      } else if (text == "/quote") { // для какой-нибудь фразы
          msg.text = "<b>Просто текст</b>";
          bot.sendMessage(msg);
      } else if (text == "/menu") {
          msg.text = "<b>Главное меню</b>";

          // меню, для кнопок - \n-раделитель строк, ;-разделитель кнопок в одной строке
          fb::Menu menu;
          menu.text = "Погода 🌤️\nСчетчики 🔢\nСвет 💡";
          menu.resize = 1;
          menu.placeholder = "👇 Выбери функцию 👇";
          msg.setMenu(menu);

          bot.sendMessage(msg);
      } else if (text == "/send_inline_menu") {
          msg.text = "<b>Инлайн меню</b>";
          fb::InlineMenu menu(
              "kb 1 ; kb 2 ; kb 3 \n kb 4 ; kb 5",
              "test;pest;lol;https://www.google.ru/;https://www.yandex.ru/"
          );
          msg.setInlineMenu(menu);

          bot.sendMessage(msg);
      } else if (text == "/temp" || text == "Погода 🌤️") {
          String weather = getWeather("Astana"); // город для погоды
          msg.text = weather;
          bot.sendMessage(msg);
      } else if (text == "/close") {
          msg.text = "<b>⭕ Операция отменена!</b>";
          msg.removeMenu();
          bot.sendMessage(msg);
      } else {
          msg.text = "<b>🚫 Операция недоступна!</b>\n\n"+text;
          msg.removeMenu();
          bot.sendMessage(msg);
      }
  }
}

void setup() {
    // ==============
    Serial.begin(115200);
    Serial.println();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected");
    
    // Set your timezone
    configTime(0, 0, "pool.ntp.org");

    // ==============
    // attach
    bot.attachUpdate(updateh);   // подключить обработчик обновлений

    // системные настройки запуска
    bot.setToken(BOT_TOKEN);
    // пропуск обновлений которые пришли до запуска бота
    bot.skipUpdates();
    // установка поллинга 4000-Default
    bot.setPollMode(fb::Poll::Long, 4000);
    
    // стартовое сообщение
    bot.sendMessage(fb::Message("✅ Бот успешно запущен!", CHAT_ID));
}

void loop() {
    bot.tick();
}
