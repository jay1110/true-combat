"""Deterministic weather/cache/argv tests; never contact an API or launch ET."""
import json
from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'launcher'))
import start_game as w


class WeatherLauncher(unittest.TestCase):
    now=1794000000
    def fetch(self,url):
        if url.startswith(w.GEOCODING):return {'results':[{'name':'Berlin','latitude':52.5,'longitude':13.4,'country_code':'DE'}]}
        return {'utc_offset_seconds':3600,'current':{'time':self.now,'weather_code':71,'snowfall':.2},
                'hourly':{'time':[self.now,self.now+3600],'snow_depth':[0,0],'snowfall':[0,0]},
                'daily':{'sunrise':[self.now//86400*86400+6*3600],'sunset':[self.now//86400*86400+17*3600]}}

    def test_real_request_parser_and_cvar_arguments(self):
        value=w.request_weather('Berlin','DE',self.fetch)
        self.assertTrue(w.usable(value,'Berlin','DE',self.now));self.assertEqual(value['snow'],1)
        args=w.weather_arguments(value);self.assertIn('ui_backgroundWeatherStamp',args)
        self.assertEqual(args[args.index('ui_backgroundSunrise')+1],'420')
        value['location']='bad";+quit\nName';self.assertEqual(w.weather_arguments(value)[-1],'badquitName')

    def test_fresh_cache_exact_location_stale_and_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            cache=Path(temp)/'weather.json'
            value,status=w.resolve_weather('Berlin','DE',cache,self.now,fetch=self.fetch)
            self.assertEqual(status,'live-weather');self.assertTrue(cache.is_file())
            def fail(url):raise OSError('offline')
            self.assertEqual(w.resolve_weather('Berlin','DE',cache,self.now+20,fetch=fail)[1],'fresh-cache')
            self.assertIsNone(w.resolve_weather('Berlin','DE',cache,self.now+10801,fetch=fail)[0])
            self.assertIsNone(w.resolve_weather('Paris','FR',cache,self.now,fetch=fail)[0])
            self.assertEqual(w.resolve_weather('Berlin','DE',cache,self.now+2000,fetch=fail)[1],'fresh-cache-after-OSError')

    def test_invalid_weather_never_sets_snow(self):
        with tempfile.TemporaryDirectory() as temp:
            def invalid(url):return {'results':[]} if url.startswith(w.GEOCODING) else {}
            value,status=w.resolve_weather('missing','DE',Path(temp)/'c',self.now,fetch=invalid)
            self.assertIsNone(value);self.assertEqual(w.weather_arguments(value),['+set','ui_backgroundWeatherStamp','0'])
        value=w.request_weather('Berlin','DE',self.fetch);value['stamp']=self.now+1
        self.assertFalse(w.usable(value,'Berlin','DE',self.now))

    def test_rain_is_not_snow_and_polar_times_use_clock_defaults(self):
        def rain(url):
            value=self.fetch(url)
            if 'current' in value:value['current'].update(weather_code=63,snowfall=0);value['daily']={'sunrise':[None],'sunset':[None]}
            return value
        value=w.request_weather('Berlin','DE',rain)
        self.assertEqual((value['snow'],value['sunrise'],value['sunset']),(0,420,1140))

    def test_existing_snow_and_short_forecast_without_current_snow(self):
        for ground,coming,reason in ((.12,0,'ground-snow'),(0,.2,'forecast-snow-within-3h'),(0,0,'no-snow-evidence')):
            def weather(url):
                value=self.fetch(url)
                if 'current' in value:
                    value['current'].update(weather_code=0,snowfall=0)
                    value['hourly'].update(snow_depth=[ground,0],snowfall=[0,coming])
                return value
            result=w.request_weather('Berlin','DE',weather)
            self.assertEqual(result['snow_reason'],reason);self.assertEqual(result['snow'],int(ground>0 or coming>0))


if __name__=='__main__':unittest.main()
