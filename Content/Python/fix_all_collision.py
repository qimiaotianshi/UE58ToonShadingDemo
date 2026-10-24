"""批量给场景里的实体网格生成凸分解碰撞（房子/地面/道路/石头等）。

原理：地形 tile 之所以能站人，是因为它有 8 个 Convex 简单碰撞；
其他网格的 agg_geom 全是 0，所以角色直接穿过去。
这里给它们补上同样的凸分解碰撞。
"""
import unreal

TAG = '[FixAll] '
HULLS, VERTS, PRECISION = 8, 16, 5
LIMIT = 150
# 只给"实体"补碰撞，植被/草/树不补（否则会挡住路）
WHITELIST = ('SM_Old_Bui_', 'SM_Old_Gro_', 'SM_Old_Roa_', 'SM_Old_Sto_',
             'SM_Old_Box_', 'SM_Old_Fen_', 'SM_Old_Wal_', 'SM_Old_Car_',
             'SM_Old_Bri_', 'SM_Old_Wat_')


def log(m):
    unreal.log(TAG + str(m))


def p(o, n, d=None):
    try:
        return o.get_editor_property(n)
    except Exception:
        return d


def convex_count(mesh):
    body = p(mesh, 'body_setup')
    if body is None:
        return -1
    agg = p(body, 'agg_geom')
    if agg is None:
        return -1
    try:
        return len(p(agg, 'convex_elems') or [])
    except Exception:
        return -1


def main():
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    meshes = {}
    for a in eas.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = p(c, 'static_mesh')
            if m is None:
                continue
            n = m.get_name()
            if n.startswith(WHITELIST) and n not in meshes:
                meshes[n] = m

    log('场景里匹配到的实体网格: {} 个'.format(len(meshes)))

    todo = []
    for n, m in sorted(meshes.items()):
        if convex_count(m) <= 0:
            todo.append((n, m))
    log('其中没有简单碰撞的: {} 个，本次处理前 {} 个'.format(len(todo), LIMIT))

    done, fail = 0, 0
    for i, (n, m) in enumerate(todo[:LIMIT]):
        try:
            sub.set_convex_decomposition_collisions(m, HULLS, VERTS, PRECISION)
            c = convex_count(m)
            if c > 0:
                done += 1
                log('   [{}/{}] OK  {}  convex={}'.format(i + 1, LIMIT, n, c))
            else:
                fail += 1
                log('   [{}/{}] 空  {}  convex=0'.format(i + 1, LIMIT, n))
        except Exception as e:
            fail += 1
            log('   [{}/{}] 异常 {}: {}'.format(i + 1, LIMIT, n, str(e)[:80]))

    log('-' * 50)
    log('成功 {}  失败/空 {}'.format(done, fail))

    try:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        log('已保存 dirty 资产')
    except Exception as e:
        log('保存失败: {}'.format(e))

    log('=' * 50)


main()
