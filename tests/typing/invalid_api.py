import numpy as np
from numpy.typing import NDArray
import qupathxx as q

wrong: NDArray[np.float32]=np.zeros((4,4,3),dtype=np.float32)
q.Image.from_numpy(wrong)
q.Project().document('wrong image id')
q.Image.demo().width=4
q.ProjectBatchJob().request(q.Image.demo(),'results')

q.ProjectBatchReport()
