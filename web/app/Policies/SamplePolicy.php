<?php

namespace App\Policies;

use App\Models\Sample;
use App\Models\User;

class SamplePolicy
{
    public function view(User $user, Sample $sample): bool
    {
        return $user->isLabManager() || $sample->run?->user_id === $user->id;
    }
}
