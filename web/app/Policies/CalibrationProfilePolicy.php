<?php

namespace App\Policies;

use App\Models\CalibrationProfile;
use App\Models\User;

class CalibrationProfilePolicy
{
    public function viewAny(User $user): bool
    {
        return true;
    }

    public function view(User $user, CalibrationProfile $calibration): bool
    {
        return true;
    }

    public function create(User $user): bool
    {
        return $user->isLabManager();
    }

    public function update(User $user, CalibrationProfile $calibration): bool
    {
        return $user->isLabManager();
    }

    public function activate(User $user, CalibrationProfile $calibration): bool
    {
        return $user->isLabManager();
    }
}
