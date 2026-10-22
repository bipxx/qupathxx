"""Run a Python producer while the main thread owns the ImGui viewer.

python live_workflow.py study.qpxproj output
Requires a GUI-enabled wheel. The viewer owns its separate live native model.
"""
import argparse
from pathlib import Path
import threading

import qupathxx as q


def main() -> None:
    ap=argparse.ArgumentParser();ap.add_argument('project',type=Path);ap.add_argument('output',type=Path);args=ap.parse_args()
    output=args.output/'scripted.qpxproj';table=args.output/'scripted.csv'
    if output.exists() or table.exists():ap.error('choose an output directory without scripted.qpxproj or scripted.csv')
    args.output.mkdir(parents=True,exist_ok=True);session=q.ViewerSession(str(args.project.resolve()));errors:list[BaseException]=[]
    def done(ticket:int) -> q.ViewerEvent:
        event=session.wait(ticket,60)
        if event.state!='succeeded':raise RuntimeError(f'{event.action}: {event.state}: {event.error}')
        return session.take(ticket)
    def producer() -> None:
        try:
            catalog=done(session.catalog())
            if not catalog.images:raise ValueError('the project has no images')
            image=catalog.images[0];width,height=min(image.width,512),min(image.height,512)
            created=done(session.execute(image.id,[q.ObjectEdit.create(q.Rect(0,0,width,height),'Python ROI')]))
            obj=created.object_ids[0];done(session.activate(image.id));done(session.select(image.id,[obj]))
            result=done(session.analyze(image.id,obj,q.StainTransform([[.65,.70,.29],[.07,.99,.11]])))
            done(session.export_measurements(image.id,q.MeasurementTableKind.ROI,str(table),record_ids=[result.record_id]))
            done(session.save(str(output)));print(f'Saved ROI {obj}, record {result.record_id}: {output}')
        except BaseException as error:errors.append(error)
        finally:
            try:session.close()
            except RuntimeError:pass
    worker=threading.Thread(target=producer,daemon=True);worker.start()
    try:q.show(session)
    finally:worker.join(65)
    if worker.is_alive():raise RuntimeError('producer did not finish')
    if errors:raise errors[0]


if __name__=='__main__':main()
