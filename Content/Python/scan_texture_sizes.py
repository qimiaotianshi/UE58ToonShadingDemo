"""统计 Textures 目录下所有贴图的分辨率分布（只读）。"""
import unreal

TAG = '[TexSize] '
ROOT = '/Game/Stage/Suibo/Textures'


def log(m):
    unreal.log(TAG + str(m))


def main():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = ar.get_assets_by_path(ROOT, True)
    log('目录 {}  资产总数 {}'.format(ROOT, len(assets)))

    dist = {}
    by_dir = {}
    unknown = 0
    total = 0

    for a in assets:
        cls = str(a.asset_class_path.asset_name) if a.asset_class_path else '?'
        pkg = str(a.package_name)
        sub = 'Arrays' if '/Arrays/' in pkg else (
            'ArraySlices' if '/ArraySlices/' in pkg else 'root')
        try:
            tex = a.get_asset()
        except Exception:
            tex = None
            try:
                tex = unreal.EditorAssetLibrary.load_asset(pkg)
            except Exception:
                pass
        if tex is None:
            unknown += 1
            continue
        try:
            sx = int(tex.blueprint_get_size_x())
            sy = int(tex.blueprint_get_size_y())
        except Exception:
            unknown += 1
            continue
        if sx <= 0 or sy <= 0:
            unknown += 1
            continue
        key = max(sx, sy)
        dist[key] = dist.get(key, 0) + 1
        d = by_dir.setdefault(sub, {})
        d[key] = d.get(key, 0) + 1
        total += 1

    log('读到尺寸的贴图: {}   读不到的: {}'.format(total, unknown))
    log('-' * 58)
    log('按最大边分辨率分布:')
    for k in sorted(dist.keys(), reverse=True):
        log('   {:>6} px : {:>4} 张'.format(k, dist[k]))
    log('-' * 58)
    for sub, d in by_dir.items():
        parts = ', '.join('{}px={}'.format(k, v) for k, v in sorted(d.items(), reverse=True))
        log('   {:<12} {}'.format(sub, parts))
    log('=' * 58)


main()
