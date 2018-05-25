#ifndef __H_CHAINSQL_VM_EXECUTIVE_H__
#define __H_CHAINSQL_VM_EXECUTIVE_H__

namespace ripple {
class Executive {
public:
	Executive();
	~Executive();
	
	void initialize();
	void finalize();
	bool execute();
	bool create();
	bool call();
private:
};
} // namespace ripple

#endif // !__H_CHAINSQL_VM_EXECUTIVE_H__
