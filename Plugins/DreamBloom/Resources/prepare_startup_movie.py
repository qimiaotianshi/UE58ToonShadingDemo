"""Prepare the loose MP4 used by the native startup MoviePlayer; leave the source untouched."""
from pathlib import Path
import argparse, json, shutil, subprocess

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project',required=True,help='Project folder containing Content')
    parser.add_argument('--input',required=True,help='Source MP4, absolute or relative to the project')
    parser.add_argument('--fade',type=float,default=1.5,help='Ending picture/audio fade in seconds')
    parser.add_argument('--ffmpeg',help='Optional full path to ffmpeg.exe')
    parser.add_argument('--name',default='StartGameAnimation.mp4',help='Output name under Content/Movies')
    args=parser.parse_args()
    project=Path(args.project).resolve()
    source=Path(args.input);source=(source if source.is_absolute() else project/source).resolve()
    movies=(project/'Content/Movies').resolve();movies.mkdir(parents=True,exist_ok=True)
    output=(movies/args.name).resolve()
    if not output.is_relative_to(movies) or output.suffix.lower()!='.mp4' or source==output:
        raise SystemExit('Output must be a separate MP4 inside Content/Movies.')
    if not source.is_file():raise SystemExit('Input MP4 does not exist.')
    found=list((project/'Content/AI').glob('**/runtime/ffmpeg.exe'))
    ffmpeg=Path(args.ffmpeg) if args.ffmpeg else Path(shutil.which('ffmpeg') or (str(found[0]) if found else ''))
    if not ffmpeg.is_file():raise SystemExit('Provide --ffmpeg or install ffmpeg on PATH.')
    probe=ffmpeg.with_name('ffprobe.exe' if ffmpeg.suffix=='.exe' else 'ffprobe')
    info=json.loads(subprocess.check_output([str(probe),'-v','error','-show_entries','format=duration:stream=codec_type','-of','json',str(source)]))
    duration=float(info['format']['duration']);fade=max(0.,min(args.fade,duration));start=max(0.,duration-fade)
    command=[str(ffmpeg),'-hide_banner','-loglevel','error','-y','-i',str(source)]
    if fade:
        video_filter=f'fade=t=out:st={start:.6f}:d={fade:.6f}'
        font=project/'Plugins/DreamBloom/Resources/NotoSansSC-Regular.ttf'
        if font.is_file():
            font_path=font.as_posix().replace(':',r'\:')
            alpha=f"if(gte(t,{start:.6f}),min(1,(t-{start:.6f})/0.5)*(0.25+0.75*(1+cos(2.5*t))/2),0)"
            video_filter+=f",drawtext=fontfile='{font_path}':text='点击运行游戏':fontsize=h*0.031:fontcolor=0xFFB8D4:x=(w-tw)/2:y=h*0.84:alpha='{alpha}'"
        command+=['-vf',video_filter]
        if any(s['codec_type']=='audio' for s in info['streams']):command+=['-af',f'afade=t=out:st={start:.6f}:d={fade:.6f}']
    # The local GPT-SoVITS FFmpeg build has Media Foundation but no GPL libx264.
    command+=['-c:v','h264_mf','-b:v','8000k','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k','-movflags','+faststart',str(output)]
    subprocess.run(command,check=True)
    output.with_suffix(output.suffix+'.startup.json').write_text(json.dumps({'duration':duration,'fade_time':fade,'fade_baked':True},indent=2),encoding='utf-8')
    print(json.dumps({'movie':str(output),'DA_StartupMovie':output.relative_to(project).as_posix(),'duration':duration,'fade':fade},ensure_ascii=False))

if __name__=='__main__':main()
