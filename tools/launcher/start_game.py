#!/usr/bin/env python3
"""Optional ET launcher: local daylight/snow background from explicit location.

The mod itself uses only stock ET traps. HTTPS is performed here, never in a
render frame. Without a location/fresh result the game uses its local clock;
the launcher never presents seasonal guesses as observed snowfall.
"""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.parse
import urllib.request

FRESH_SECONDS = 10800
SNOW_CODES = {71,73,75,77,85,86}
GEOCODING = 'https://geocoding-api.open-meteo.com/v1/search'
FORECAST = 'https://api.open-meteo.com/v1/forecast'


def fetch_json(url):
    if urllib.parse.urlsplit(url).scheme != 'https':
        raise ValueError('Weather requests require HTTPS')
    request=urllib.request.Request(url,headers={'User-Agent':'TCE2-menu-background/1.0.3'})
    with urllib.request.urlopen(request,timeout=4) as response:
        if urllib.parse.urlsplit(response.geturl()).scheme != 'https':
            raise ValueError('Insecure weather redirect')
        raw=response.read(512*1024+1)
        if len(raw)>512*1024:raise ValueError('Unbounded weather response')
    value=json.loads(raw)
    if not isinstance(value,dict) or value.get('error'):raise ValueError('Weather service rejected the request')
    return value


def location_key(location,country):
    return location.strip().casefold()+'|'+country.strip().upper()


def request_weather(location,country='',fetch=fetch_json):
    if not 2<=len(location.strip())<=120 or (country and (len(country)!=2 or not country.isalpha())):
        raise ValueError('Use a city/postal code and optional two-letter country code')
    query={'name':location.strip(),'count':5,'language':'de','format':'json'}
    if country:query['countryCode']=country.upper()
    results=fetch(GEOCODING+'?'+urllib.parse.urlencode(query)).get('results',[])
    if not isinstance(results,list) or not results:raise ValueError('No matching location')
    place=results[0]
    latitude,longitude=float(place['latitude']),float(place['longitude'])
    if not math.isfinite(latitude) or not math.isfinite(longitude) or not -90<=latitude<=90 or not -180<=longitude<=180:
        raise ValueError('Invalid location coordinates')
    query={'latitude':latitude,'longitude':longitude,'current':'weather_code,snowfall',
           'hourly':'snow_depth,snowfall','daily':'sunrise,sunset',
           'timezone':'auto','timeformat':'unixtime','forecast_days':2}
    data=fetch(FORECAST+'?'+urllib.parse.urlencode(query))
    current=data['current'];offset=int(data['utc_offset_seconds']);stamp=int(current['time'])
    code=int(current['weather_code']);snowfall=float(current['snowfall'])
    if not -50400<=offset<=50400 or not math.isfinite(snowfall) or snowfall<0:
        raise ValueError('Invalid weather values')
    hourly=data['hourly'];times=hourly['time'];depths=hourly['snow_depth'];falls=hourly['snowfall']
    if not 1<=len(times)<=72 or len(times)!=len(depths) or len(times)!=len(falls):
        raise ValueError('Invalid hourly snow series')
    values=[(int(t),float(d),float(f)) for t,d,f in zip(times,depths,falls)]
    if any(not math.isfinite(d) or not math.isfinite(f) or d<0 or f<0 for _,d,f in values):
        raise ValueError('Invalid hourly snow amount')
    nearest=min(values,key=lambda row:abs(row[0]-stamp))
    depth=nearest[1] if abs(nearest[0]-stamp)<=3600 else 0
    coming_snow=max([f for t,_,f in values if stamp<=t<=stamp+10800] or [0])
    snow_reason=('ground-snow' if depth>0 else 'current-snow' if code in SNOW_CODES or snowfall>0
                 else 'forecast-snow-within-3h' if coming_snow>0 else 'no-snow-evidence')
    daily=data['daily'];rise=daily['sunrise'][0];setting=daily['sunset'][0]
    # Polar sunrise/sunset can be absent. Retain genuine weather but let the
    # mod use documented fixed clock windows for the unavailable solar times.
    sunrise=((int(rise)+offset)%86400)//60 if rise is not None else 420
    sunset=((int(setting)+offset)%86400)//60 if setting is not None else 1140
    return {'schema':1,'key':location_key(location,country),'location':str(place['name'])[:80],
            'country':str(place.get('country_code',''))[:2],'stamp':stamp,'offset':offset,
            'sunrise':sunrise,'sunset':sunset,'snow':int(snow_reason!='no-snow-evidence'),
            'snow_reason':snow_reason,'snow_depth_m':depth,'snowfall_next3h_cm':coming_snow,
            'weather_code':code,'source':'Open-Meteo','source_url':'https://open-meteo.com/en/docs'}


def usable(value,location,country,now):
    try:
        return (value['schema']==1 and value['key']==location_key(location,country) and
                isinstance(value['location'],str) and 0<len(value['location'])<=80 and
                all(type(value[x]) is int for x in ('stamp','offset','sunrise','sunset','snow')) and
                0<=now-value['stamp']<=FRESH_SECONDS and -50400<=value['offset']<=50400 and
                0<=value['sunrise']<1440 and 0<=value['sunset']<1440 and value['snow'] in (0,1))
    except (KeyError,TypeError,ValueError):return False


def resolve_weather(location,country,cache,now=None,network=True,fetch=fetch_json):
    now=int(time.time()) if now is None else int(now);cache=Path(cache)
    previous=None
    try:
        if cache.stat().st_size<=16384:previous=json.loads(cache.read_text(encoding='utf8'))
    except (OSError,ValueError):pass
    if usable(previous,location,country,now) and now-previous['stamp']<1800:
        return previous,'fresh-cache'
    error='network-disabled'
    if network:
        try:
            value=request_weather(location,country,fetch)
            if not usable(value,location,country,now):raise ValueError('Weather result is stale or malformed')
            cache.parent.mkdir(parents=True,exist_ok=True)
            temporary=cache.with_name(cache.name+'.tmp');temporary.write_text(json.dumps(value,indent=2)+'\n',encoding='utf8')
            temporary.replace(cache)
            return value,'live-weather'
        except (OSError,ValueError,KeyError,IndexError,TypeError) as exc:error=type(exc).__name__
    if usable(previous,location,country,now):return previous,'fresh-cache-after-'+error
    return None,'local-clock/no-fresh-weather/'+error


def weather_arguments(value):
    if value is None:return ['+set','ui_backgroundWeatherStamp','0']
    result=[]
    for cvar,key in [('WeatherStamp','stamp'),('UtcOffset','offset'),('Sunrise','sunrise'),
                     ('Sunset','sunset'),('Snow','snow')]:
        result+=['+set','ui_background'+cvar,str(value[key])]
    # Labels are never console commands. Keep only inert printable characters
    # as argv still enters ET's command-line parser after process creation.
    label=''.join(c for c in value['location'] if c.isalnum() or c in ' _-')[:80] or 'configured-location'
    result+=['+set','ui_backgroundLocation',label]
    return result


def default_cache():
    base=Path(os.environ.get('LOCALAPPDATA',Path.home()/'.cache'))
    return base/'TCE2'/'background-weather.json'


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game',type=Path,required=True,help='Existing et.exe/etl.exe/et.x86 executable')
    p.add_argument('--location',default='',help='Explicit city or postal code; no IP geolocation')
    p.add_argument('--country',default='',help='Optional ISO country code, e.g. DE')
    p.add_argument('--cache',type=Path,default=default_cache())
    p.add_argument('--no-network',action='store_true',help='Use only matching fresh cache')
    p.add_argument('--print-only',action='store_true',help='Resolve/report without starting ET')
    p.add_argument('game_args',nargs=argparse.REMAINDER)
    a=p.parse_args(argv);game=a.game.expanduser().resolve()
    if not game.is_file():p.error('Game executable does not exist: '+str(game))
    value,status=resolve_weather(a.location,a.country,a.cache,network=not a.no_network) if a.location else (None,'local-clock/no-location')
    extra=a.game_args[1:] if a.game_args[:1]==['--'] else a.game_args
    command=[str(game),*extra,*weather_arguments(value)]
    report={'status':status,'location':value['location'] if value else None,'command':command,
            'weather_source':'https://open-meteo.com/en/docs' if value else None,
            'snow_evidence':value.get('snow_reason') if value else None}
    if not a.print_only:report['pid']=subprocess.Popen(command,cwd=game.parent).pid
    print(json.dumps(report,ensure_ascii=False));return 0


if __name__=='__main__':raise SystemExit(main())
