#include <textMapper.h>
long mapToSource(long resultPos, const strTextModify edits,long startEdit) {
	for (long i = strTextModifySize(edits) - 1; i >= startEdit; i--) {
		__auto_type edit = &edits[i];
		if (resultPos >= edit->where) {
			if (edit->type == MODIFY_INSERT) {
				if (edit->where <= resultPos) {
					if (edit->where + edit->len >= resultPos)
						resultPos = edit->where;
					else
						resultPos -= edit->len;
				}
			} else if (edit->type == MODIFY_REMOVE) {
				if (edit->where < resultPos) {
					resultPos += edit->len;
				}
			}
		}
	}
	return resultPos;
}
