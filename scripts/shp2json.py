import geopandas

shpfile = geopandas.read_file('Allevamenti.shp')
shpfile.to_file('Banchi_agg_ott2024.geojson', driver='GeoJSON')