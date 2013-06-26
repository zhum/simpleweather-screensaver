#define MAX_LANGUAGES 1
#define MAX_CONDITIONS 48

#define NO_CODE -1024

const char *codes[MAX_LANGUAGES+1][MAX_CONDITIONS+1]={
//english
{
//-1
"Unknown",
//0
"tornado",
"tropical storm",
"hurricane",
"severe thunderstorms",
"thunderstorms",
"mixed rain and snow",
"mixed rain and sleet",
"mixed snow and sleet",
"freezing drizzle",
"drizzle",
//10
"freezing rain",
"showers",
"showers",
"snow flurries",
"light snow showers",
"blowing snow",
"snow",
"hail",
"sleet",
"dust",
//20
"foggy",
"haze",
"smoky",
"blustery",
"windy",
"cold",
"cloudy",
"mostly cloudy",
"mostly cloudy",
"partly cloudy",
//30
"partly cloudy",
"clear",
"sunny",
"fair",
"fair",
"mixed rain and hail",
"hot",
"isolated thunderstorms",
"scattered thunderstorms",
"scattered thunderstorms",
//40
"scattered showers",
"heavy snow",
"scattered snow showers",
"heavy snow",
"partly cloudy",
"thundershowers",
"snow showers",
"isolated thundershowers"},

//russian
{
//-1
"Неизвестно",
//0
"Торнадо",
"Шторм",
"Ураган",
"Временами грозы",
"Грозы",
"Снег с дождём",
"Дождь, слякоть",
"Снег, слякоть",
"Морось с гололедицей",
"Моросящий дождь",
//10
"Дождь с гололедицей",
"Дождь",
"Дождь",
"Снегопад",
"Лёгкий снег",
"Ветер со снегом",
"Снег",
"Град",
"Слякоть",
"Пыль",
//20
"Туман",
"Лёгкий туман",
"Дым",
"Ветренно",
"Ветер",
"Холод",
"Облачно",
"Преимущественно облачно",
"Преимущественно облачно",
"Временами облачно",
//30
"Временами облачно",
"Ясно",
"Солнечно",
"Преимущественно ясно",
"Преимущественно ясно",
"Дождь с градом",
"Жара",
"Местами грозы",
"Местами грозы",
"Местами грозы",
//40
"Местами дожди",
"Снегопад",
"Местами снег с дождём",
"Снегопад",
"Местами облачно",
"Гроза",
"Снег с дождём",
"Местами грозы"}
};
