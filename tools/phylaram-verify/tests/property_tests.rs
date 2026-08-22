use proptest::prelude::*;

#[derive(Debug, Clone, PartialEq, Eq)]
struct PhysicalRun {
    start: u64,
    length: u64,
}

fn validate_and_sum(runs: &mut [PhysicalRun]) -> Result<(u64, u64), &'static str> {
    if runs.is_empty() {
        return Err("empty runs");
    }

    runs.sort_by_key(|r| r.start);

    let mut total_bytes = 0u64;
    let mut highest_end = 0u64;
    let mut prev_end = 0u64;

    for (i, r) in runs.iter().enumerate() {
        if r.length == 0 {
            return Err("zero length");
        }
        if r.start.checked_add(r.length).is_none() {
            return Err("overflow range");
        }
        let end = r.start + r.length;
        if i > 0 && r.start < prev_end {
            return Err("overlap");
        }
        total_bytes = total_bytes.checked_add(r.length).ok_or("overflow total")?;
        if end > highest_end {
            highest_end = end;
        }
        prev_end = end;
    }

    Ok((total_bytes, highest_end))
}

proptest! {
    #[test]
    fn test_valid_disjoint_runs_always_sum(
        starts in proptest::collection::vec(0u64..1_000_000_000, 1..20),
        lengths in proptest::collection::vec(1u64..100_000, 1..20)
    ) {
        let count = std::cmp::min(starts.len(), lengths.len());
        let mut runs = Vec::new();
        let mut cur_offset = 0u64;

        for i in 0..count {
            let start = cur_offset + (starts[i] % 10_000);
            let length = lengths[i];
            runs.push(PhysicalRun { start, length });
            cur_offset = start + length + 100; // strictly disjoint
        }

        let res = validate_and_sum(&mut runs);
        prop_assert!(res.is_ok());
        let (total, highest) = res.unwrap();
        prop_assert!(total > 0);
        prop_assert!(highest >= total);
    }

    #[test]
    fn test_overlapping_runs_always_rejected(
        start in 0u64..1_000_000,
        len1 in 100u64..10_000,
        overlap in 1u64..50
    ) {
        let mut runs = vec![
            PhysicalRun { start, length: len1 },
            PhysicalRun { start: start + len1 - overlap, length: 1000 },
        ];
        let res = validate_and_sum(&mut runs);
        prop_assert!(res.is_err());
    }

    #[test]
    fn test_accounting_invariant_holds(
        physical in 1000u64..100_000_000,
        unreadable in 0u64..1000
    ) {
        let actual_unreadable = std::cmp::min(physical, unreadable);
        let acquired = physical - actual_unreadable;
        prop_assert_eq!(acquired + actual_unreadable, physical);
    }
}
