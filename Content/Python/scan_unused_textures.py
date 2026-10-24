"""只读扫描：找出 Suibo/Textures 下没有被任何资产引用的贴图（v2）。"""
import unreal
import os

TAG = '[UnusedTex] '
PKG_PATH = '/Game/Stage/Suibo/Textures'
CONTENT_ROOT = 'D:/UE/UE58ToonShadingDemo/Toon58ShadingDemo/Content'
OUT_TXT = 'D:/UE/UE58ToonShadingDemo/Toon58ShadingDemo/Saved/unused_textures.txt'
SHOW = 40


def log(m):
    unreal.log(TAG + str(m))


def disk_size(package_path):
    rel = package_path.split('/Game/', 1)[-1]
    head, _, tail = rel.rpartition('/')
    name = tail.split('.')[0]
    fp = os.path.join(CONTENT_ROOT, head.replace('/', os.sep), name + '.uasset')
    try:
        return os.path.getsize(fp) if os.path.isfile(fp) else 0
    except Exception:
        return 0


def main():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = ar.get_assets_by_path(PKG_PATH, True)
    log('目录 {}  资产总数 {}'.format(PKG_PATH, len(assets)))

    el = unreal.EditorAssetLibrary
    unused, used, errs = [], 0, 0
    for i, a in enumerate(assets):
        pkg = str(a.package_name)
        try:
            refs = el.find_package_referencers_for_asset(pkg, False)
        except Exception:
            errs += 1
            continue
        if refs and len(refs) > 0:
            used += 1
        else:
            unused.append((disk_size(pkg), pkg))
        if (i + 1) % 400 == 0:
            log('   进度 {}/{}'.format(i + 1, len(assets)))

    unused.sort(key=lambda x: -x[0])
    total = sum(u[0] for u in unused)
    log('-' * 62)
    log('有引用 {}   无引用 {}   异常 {}'.format(used, len(unused), errs))
    log('无引用合计 {:.1f} MB'.format(total / 1024.0 / 1024.0))
    log('-' * 62)
    for size, pkg in unused[:SHOW]:
        log('   {:8.2f} MB  {}'.format(size / 1024.0 / 1024.0, pkg))
    if len(unused) > SHOW:
        log('   ... 另有 {} 个（见清单文件）'.format(len(unused) - SHOW))

    try:
        with open(OUT_TXT, 'w', encoding='utf-8') as f:
            f.write('# {}\n'.format(PKG_PATH))
            f.write('# total={} unused={} size={:.1f}MB\n'.format(
                len(assets), len(unused), total / 1024.0 / 1024.0))
            for size, pkg in unused:
                f.write('{:.2f}\t{}\n'.format(size / 1024.0 / 1024.0, pkg))
        log('清单已写入 {}'.format(OUT_TXT))
    except Exception as e:
        log('写清单失败: {}'.format(e))

    log('=' * 62)


main()
